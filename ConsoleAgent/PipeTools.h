
#include <Windows.h>
#include <atlbase.h>

#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>

using namespace std;

typedef vector<char> buffer_t;

class PipeWriter
{
public:
	PipeWriter() = delete;

	PipeWriter(shared_ptr<ATL::CHandle> hPipe)
	{
		mPipe = hPipe;

		mWriteThreadShouldRun = true;
		mWriteThread = thread(&PipeWriter::WriteThreadFunc, this);
	}

	virtual ~PipeWriter()
	{
		if (mWriteThreadShouldRun)
		{
			mWriteThreadShouldRun = false;
			mWriteThreadEvent.notify_all();
			mWriteThread.join();
		}
	}

	void Write(const buffer_t &data)
	{
		lock_guard<mutex> lck(mQueueMutex);
		mQueue.push_back(data);
		mWriteThreadEvent.notify_all();
	}

protected:

	void WriteThreadFunc()
	{
		while (mWriteThreadShouldRun)
		{
			buffer_t buf = GetNextBuffer();
			WriteToPipe(buf);
		}
	}

	void WriteToPipe(const buffer_t &buf)
	{
		DWORD bytesWritten;

		const char *start = buf.data();
		DWORD remaining = (DWORD)buf.size();

		while (remaining > 0 && mWriteThreadShouldRun)
		{
			WriteFile(*mPipe, start, remaining, &bytesWritten, NULL);
			start += bytesWritten;
			remaining -= bytesWritten;
		}
	}

	const buffer_t GetNextBuffer()
	{
		unique_lock<mutex> lck(mQueueMutex);
		while (mQueue.empty())
		{
			mWriteThreadEvent.wait(lck);
			if (!mWriteThreadShouldRun)
				return buffer_t();
		}
		buffer_t buf = mQueue.front();
		mQueue.pop_front();
		return buf;
	}


	shared_ptr<ATL::CHandle> mPipe;

	deque<buffer_t> mQueue;
	mutex mQueueMutex;

	thread mWriteThread;
	volatile bool mWriteThreadShouldRun;
	condition_variable mWriteThreadEvent;
};



enum PipeInputDetect
{
	INPUTDETECT_WAIT,
	INPUTDETECT_PEEK,
	INPUTDETECT_CONSOLE,
	INPUTDETECT_NONE
};

class PipeReader
{
public:
	PipeReader() = delete;

	PipeReader(shared_ptr<ATL::CHandle> hPipe, PipeInputDetect inputDetect, bool asyncRead, size_t bufferSize=1000)
	{
		mPipe = hPipe;
		mInputDetect = inputDetect;
		mAsyncRead = asyncRead;
		mBufferSize = bufferSize;
		
		if (mAsyncRead)
		{
			mReadThreadShouldRun = true;
			mReadThread = thread(&PipeReader::ReadThreadFunc, this);
		}
	}

	virtual ~PipeReader()
	{
		if (mAsyncRead && mReadThreadShouldRun)
		{
			mReadThreadShouldRun = false;
			
			if (mInputDetect == INPUTDETECT_CONSOLE)
			{
				// HACK: For some reason it is impossible to cancel IO on a console handle.
				//       Thus the only way to stop the read operation is to terminate the thread.
				HANDLE hThread = mReadThread.native_handle();
				TerminateThread(hThread, 0);
			}

			mReadThread.join();
		}
	}

	buffer_t Read()
	{
		if (mAsyncRead)
		{
			lock_guard<mutex> lck(mQueueMutex);
			if (mQueue.empty())
				return buffer_t(0);
			buffer_t data = mQueue.front();
			mQueue.pop_front();
			return data;
		}
		else
		{
			return ReadFromPipe();
		}
	}

protected:

	void ReadThreadFunc()
	{
		while (mReadThreadShouldRun)
		{
			buffer_t buf = ReadFromPipe();
			if (!buf.empty())
				PutBuffer(buf);
			this_thread::sleep_for(chrono::milliseconds(100));
		}
	}

	buffer_t ReadFromPipe()
	{
		buffer_t empty(0);

		// check if input is available
		size_t bytesToRead;
		switch (mInputDetect)
		{
		case INPUTDETECT_PEEK:
			DWORD bytesAvail;
			if (!PeekNamedPipe(*mPipe, NULL, 0, NULL, &bytesAvail, NULL))
				return empty;
			if (bytesAvail == 0)
				return empty;
			bytesToRead = min(bytesAvail, mBufferSize);
			break;
		case INPUTDETECT_WAIT:
			DWORD result;
			result = WaitForSingleObject(*mPipe, 0);
			if (result != WAIT_OBJECT_0)
				return empty;
			bytesToRead = mBufferSize;
			break;
		case INPUTDETECT_CONSOLE:
		case INPUTDETECT_NONE:
			bytesToRead = mBufferSize;
			break;
		}

		// read input
		buffer_t buffer(bytesToRead);
		DWORD bytesRead;		
		if (!ReadFile(*mPipe, buffer.data(), (DWORD)bytesToRead, &bytesRead, NULL))
		{
			LOG(ERROR) << "PipeReader: ReadFile error: 0x" << hex << GetLastError();
			return empty;
		}
		buffer.resize(bytesRead);

		return buffer;
	}

	void PutBuffer(const buffer_t &buf)
	{
		unique_lock<mutex> lck(mQueueMutex);
		mQueue.push_back(buf);
	}


	shared_ptr<ATL::CHandle> mPipe;

	deque<buffer_t> mQueue;
	mutex mQueueMutex;

	bool mAsyncRead;
	thread mReadThread;
	volatile bool mReadThreadShouldRun;

	PipeInputDetect mInputDetect;
	size_t mBufferSize;
};
