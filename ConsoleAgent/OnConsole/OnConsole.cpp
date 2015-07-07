
#include "stdafx.h"

#include "utils.h"
#include "PipeTools.h"

using namespace std;


ConsoleServerLib::IExecPtr pExec;


volatile bool ConsoleSignalPending;
volatile ConsoleServerLib::ControlEvent ConsoleSignalEvent;

// console event handler
BOOL WINAPI CtrlHandler(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
		ConsoleSignalEvent = ConsoleServerLib::CONTROLEVENT_C;
		ConsoleSignalPending = true;
		break;
	case CTRL_BREAK_EVENT:
		ConsoleSignalEvent = ConsoleServerLib::CONTROLEVENT_BREAK;
		ConsoleSignalPending = true;
		break;
	}
	return TRUE;
}



int _tmain(int argc, _TCHAR* argv[])
{
	HRESULT hr;

	CoInitialize(NULL);
	hr = CoInitializeSecurity(
		NULL,
		-1,
		NULL,
		NULL,
		RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE,
		NULL);
	if (FAILED(hr)) {
		wcerr << "COM security error: 0x" << std::hex << hr;
		return -3;
	}

	// usage
	if (argc < 2)
	{
		wcerr << "Usage: " << argv[0] << " <program> <arguments>" << endl;
		wcerr << endl;
		wcerr << "Executes the specified program on the console session." << endl;
		return -2;
	}
	
	try 
	{
		// connect to COM server
		pExec.CreateInstance(__uuidof(ConsoleServerLib::Exec));

		// build command line
		wstring cmdLine;
		for (int n = 1; n < argc; n++) 
		{
			if (n > 1)
				cmdLine += L" ";
			cmdLine += argv[n];
		}
		_bstr_t bsCmdLine(cmdLine.c_str());

		// start process on console session
		BYTE success;
		LONGLONG error;
		pExec->StartProcess(bsCmdLine.GetBSTR(), &success, &error);

		// process start failure
		if (!success)
		{
			_com_error err((HRESULT)error);
			wcerr << "Process launch failure: " << err.ErrorMessage() << endl;
			return -1;
		}
		
		// standard handles
		HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

		// check if stdin is console
		bool stdinConsole;
		DWORD consoleMode;
		if (GetConsoleMode(hStdin, &consoleMode))
			stdinConsole = true;
		else
			stdinConsole = false;

		// if stdin is a console, configure it accordingly
		if (stdinConsole)
			SetConsoleMode(hStdin, ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);

		// stdin reader
		PipeReader prStdin(hStdin, stdinConsole ? INPUTDETECT_WAIT : INPUTDETECT_NONE);

		// register console event handler
		ConsoleSignalPending = false;
		SetConsoleCtrlHandler(&CtrlHandler, TRUE);

		// monitor process
		BYTE hasTerminated;
		LONGLONG exitCode;
		do
		{
			bool hadData = false;

			// ping to show server that client is alive
			pExec->Ping();

			// check if process has terminated
			pExec->GetTerminationStatus(&hasTerminated, &exitCode);

			// receive and output stdout and stderr
			DWORD bytesWritten;
			BSTR bsBuffer;
			long bsBufferLength;

			pExec->ReadStdout(&bsBuffer, &bsBufferLength);
			WriteFile(hStdout, bsBuffer, bsBufferLength, &bytesWritten, NULL);
			SysFreeString(bsBuffer);
			if (bsBufferLength > 0)
				hadData = true;

			pExec->ReadStderr(&bsBuffer, &bsBufferLength);
			WriteFile(hStderr, bsBuffer, bsBufferLength, &bytesWritten, NULL);
			SysFreeString(bsBuffer);
			if (bsBufferLength > 0)
				hadData = true;

			// read and transmit stdin
			vector<char> vBuffer = prStdin.Read();
			if (!vBuffer.empty())
			{
				bsBuffer = VectorToBstr(vBuffer);
				pExec->WriteStdin(bsBuffer, (long)vBuffer.size());
				SysFreeString(bsBuffer);
				hadData = true;
			}

			// send any events if required
			if (ConsoleSignalPending)
			{
				pExec->SendControl(ConsoleSignalEvent);
				ConsoleSignalPending = false;
			}

			// sleep before next pooling if no data was exchanged
			if (!hadData)
				this_thread::sleep_for(chrono::milliseconds(100));
		} while (!hasTerminated);
		
		return (int)exitCode;
	}
	catch (_com_error err) {
		wcerr << "==========================================================" << endl;
		wcerr << "COM error: 0x" << hex << err.Error() << " - " << err.ErrorMessage() << endl;
		wcerr << "Check that ConsoleServer is properly installed." << endl;
		return -3;
	}

	return 0;
}

