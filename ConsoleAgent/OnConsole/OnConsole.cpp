#include <SDKDDKVer.h>
#include <tchar.h>
#include <atlbase.h>
#include <comutil.h>

#include <iostream>

#import <ConsoleServer.tlb>

#include "utils.h"
#include "PipeTools.h"


using namespace std;

INITIALIZE_EASYLOGGINGPP;


ConsoleServerLib::IExecPtr pExec;


// console events
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


void WaitForWindowStationPreparation()
{
	auto startTime = chrono::steady_clock::now();
	BYTE preparing = false;
	BYTE prepared = false;

	do
	{
		if (!preparing)
			pExec->PrepareWindowStation(&preparing);

		if (!prepared)
			pExec->IsWindowStationPrepared(&prepared);

		if (chrono::steady_clock::now() - startTime > chrono::seconds(10))
			throw runtime_error("Timeout while waiting for window station preparation.");
		this_thread::sleep_for(chrono::milliseconds(100));
	} while (!prepared);
}


void SetupRedirection(shared_ptr<PipeReader> &prStdin, 
					  shared_ptr<PipeWriter> &pwStdout, 
					  shared_ptr<PipeWriter> &pwStderr)
{
	// standard handles
	auto hStdin = make_shared<CHandle>(GetStdHandle(STD_INPUT_HANDLE));
	auto hStdout = make_shared<CHandle>(GetStdHandle(STD_OUTPUT_HANDLE));
	auto hStderr = make_shared<CHandle>(GetStdHandle(STD_ERROR_HANDLE));

	// check if stdin is console
	bool stdinConsole;
	DWORD consoleMode;
	if (GetConsoleMode(*hStdin, &consoleMode))
	{
		LOG(INFO) << "OnConsole: stdin is a console";
		stdinConsole = true;
	}
	else
	{
		LOG(INFO) << "OnConsole: stdin is redirected";
		stdinConsole = false;
	}

	// if stdin is a console, configure it accordingly
	if (stdinConsole)
		SetConsoleMode(*hStdin, ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);

	// setup pipe readers and writers
	prStdin = make_shared<PipeReader>(hStdin, stdinConsole ? INPUTDETECT_CONSOLE : INPUTDETECT_NONE, true);
	pwStdout = make_shared<PipeWriter>(hStdout);
	pwStderr = make_shared<PipeWriter>(hStderr);
}

bool HandleRedirection(shared_ptr<PipeReader> prStdin,
					   shared_ptr<PipeWriter> pwStdout,
					   shared_ptr<PipeWriter> pwStderr)
{
	bool hadData = false;
	vector<char> vBuffer;
	BSTR bsBuffer;
	long bsBufferLength;

	// read and transmit stdin
	vBuffer = prStdin->Read();
	if (!vBuffer.empty())
	{
		bsBuffer = VectorToBstr(vBuffer);
		pExec->WriteStdin(bsBuffer, (long)vBuffer.size());
		SysFreeString(bsBuffer);
		hadData = true;
	}

	// receive and output stdout 
	pExec->ReadStdout(&bsBuffer, &bsBufferLength);
	vBuffer = BstrToVector<char>(bsBuffer, bsBufferLength);
	SysFreeString(bsBuffer);
	if (!vBuffer.empty())
	{
		pwStdout->Write(vBuffer);
		hadData = true;
	}

	// receive and output stderr
	pExec->ReadStderr(&bsBuffer, &bsBufferLength);
	vBuffer = BstrToVector<char>(bsBuffer, bsBufferLength);
	SysFreeString(bsBuffer);
	if (!vBuffer.empty())
	{
		pwStderr->Write(vBuffer);
		hadData = true;
	}

	return hadData;
}

int _tmain(int argc, _TCHAR* argv[])
{
	// initialize logging
	ConfigureLogging();

	// initialize COM
	CoInitialize(NULL);
	HRESULT hr = CoInitializeSecurity(NULL,	-1,	NULL, NULL,	
									  RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
								      NULL, EOAC_NONE, NULL);
	if (FAILED(hr))
		throw runtime_error("COM security error");

	// usage
	if (argc < 2)
	{
		wcerr << "Usage: " << argv[0] << " <program> <arguments>" << endl;
		wcerr << endl;
		wcerr << "Executes the specified program on the console session." << endl;
		return -2;
	}

	// build command line
	wstring cmdLine;
	for (int n = 1; n < argc; n++)
	{
		if (n > 1)
			cmdLine += L" ";
		cmdLine += argv[n];
	}

	try 
	{
		// connect to COM server
		pExec.CreateInstance(__uuidof(ConsoleServerLib::Exec));

		// wait for preparation of window station
		WaitForWindowStationPreparation();

		// start process on console session
		BYTE success;
		LONGLONG error;
		pExec->StartProcess(WStringToBStr(cmdLine), &success, &error);

		// check if start failed
		if (!success)
		{
			_com_error err((HRESULT)error);
			wcerr << "Process launch failure: " << err.ErrorMessage() << endl;
			return -1;
		}

		// register console event handler
		ConsoleSignalPending = false;
		SetConsoleCtrlHandler(&CtrlHandler, TRUE);

		// setup console redirection
		shared_ptr<PipeReader> prStdin;
		shared_ptr<PipeWriter> pwStdout;
		shared_ptr<PipeWriter> pwStderr;
		SetupRedirection(prStdin, pwStdout, pwStderr);

		// monitor process
		BYTE hasTerminated;
		LONGLONG exitCode;
		bool hadData;
		do
		{
			// ping to show server that client is alivoe
			pExec->Ping();

			// check if process has terminated
			pExec->GetTerminationStatus(&hasTerminated, &exitCode);

			// send any events if required
			if (ConsoleSignalPending)
			{
				pExec->SendControl(ConsoleSignalEvent);
				ConsoleSignalPending = false;
			}

			// handle redirection
			hadData = HandleRedirection(prStdin, pwStdout, pwStderr);

			// sleep before next pooling if no data was exchanged
			if (!hadData)
				this_thread::sleep_for(chrono::milliseconds(100));
		} while (!hasTerminated || hadData);
		
		return (int)exitCode;
	}
	catch (_com_error err) 
	{
		wcerr << "COM error: 0x" << hex << err.Error() << " - " << err.ErrorMessage() << endl;
		wcerr << "Check that ConsoleServer is properly installed." << endl;
		return -3;
	}
	catch (runtime_error err)
	{
		wcerr << "Error: " << err.what() << endl;
		return -3;
	}

	return 0;
}

