#include "stdafx.h"

#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>

using namespace std;


typedef const vector<char> buffer_t;


class PipeWriter
{
public:
	PipeWriter() = delete;

	PipeWriter(HANDLE hPipe)
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
			WriteFile(mPipe, start, remaining, &bytesWritten, NULL);
			start += bytesWritten;
			remaining -= bytesWritten;
		}
	}

	buffer_t GetNextBuffer()
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


	HANDLE mPipe;

	deque<buffer_t> mQueue;
	mutex mQueueMutex;

	thread mWriteThread;
	volatile bool mWriteThreadShouldRun;
	condition_variable mWriteThreadEvent;
};