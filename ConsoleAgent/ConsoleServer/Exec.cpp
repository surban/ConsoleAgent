// Exec.cpp : Implementation of CExec

#include "stdafx.h"
#include "Exec.h"
#include "utils.h"
#include "Timers.h"

#include <thread>

// CExec

CExec::CExec()
{
	processStarted = false;
	mProcessKilled = false;

	Ping();
	RegisterTimer(this, std::bind(&CExec::TimerCallback, this));

	LOG(INFO) << "CExec instantiated on thread " << std::this_thread::get_id();
}

CExec::~CExec()
{
	KillProcess();
	DeregisterTimer(this);
	mStdinWriter = nullptr;

	if (processStarted)
	{
		CloseHandle(mProcess);
		CloseHandle(hThread);
		CloseHandle(mStdoutRead);
		CloseHandle(mStderrRead);
		CloseHandle(mStdinWrite);
	}

	LOG(INFO) << "CExec destructed";
}

void CExec::TimerCallback()
{
	// check if client is alive
	if ((clock() - mLastPingTime) > MAX_PING_TIME * CLOCKS_PER_SEC && !mProcessKilled)
	{
		LOG(WARNING) << "client did not ping -- terminating process";
		KillProcess();
	}

	//LOG(INFO) << "timer callback for IExec " << this;
}



STDMETHODIMP CExec::StartProcess(BSTR commandLine, BYTE *success, LONGLONG* error)
{
	_bstr_t sCommandLine(commandLine, true);

	// create pipes for stdin, stdout, stderr
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&mStdoutRead, &mStdoutWrite, &saAttr, 0))
		return E_FAIL;
	if (!SetHandleInformation(mStdoutRead, HANDLE_FLAG_INHERIT, 0))
		return E_FAIL;

	if (!CreatePipe(&mStderrRead, &mStderrWrite, &saAttr, 0))
		return E_FAIL;
	if (!SetHandleInformation(mStderrRead, HANDLE_FLAG_INHERIT, 0))
		return E_FAIL;

	if (!CreatePipe(&mStdinRead, &mStdinWrite, &saAttr, 0))
		return E_FAIL;
	if (!SetHandleInformation(mStdinWrite, HANDLE_FLAG_INHERIT, 0))
		return E_FAIL;


	if (CoImpersonateClient() != S_OK)
	{
		*error = GetLastError();
		LOG(WARNING) << "Impersonation failed.";
		return E_FAIL;
	}
	LOG(INFO) << "Impersonating client.";

	// get impersenation access token
	HANDLE hThread = GetCurrentThread();
	HANDLE hImpersonationToken;
	if (!OpenThreadToken(hThread, TOKEN_ALL_ACCESS, TRUE, &hImpersonationToken))
	{
		*error = GetLastError();
		LOG(WARNING) << "Opening thread access token failed.";
		return E_FAIL;
	}

	CoRevertToSelf();

	// get impersonation level
	SECURITY_IMPERSONATION_LEVEL silLevel;
	DWORD dwLength = sizeof(SECURITY_IMPERSONATION_LEVEL);
	if (!GetTokenInformation(hImpersonationToken, TokenImpersonationLevel, &silLevel, dwLength, &dwLength))
	{
		*error = GetLastError();
		LOG(WARNING) << "GetTokenInformation failed.";
		return E_FAIL;
	}
	LOG(INFO) << "Impersonation level is " << (int)silLevel;

	// obtain primary token
	HANDLE hPrimaryToken;
	if (!DuplicateTokenEx(hImpersonationToken, 0, NULL, SecurityImpersonation, TokenPrimary, &hPrimaryToken))
	{
		*error = GetLastError();
		LOG(WARNING) << "DuplicateTokenEx failed.";
		return E_FAIL;
	}

	// set session
	DWORD dwConsoleSessionId = WTSGetActiveConsoleSessionId();
	LOG(INFO) << "Console session id is 0x" << std::hex << dwConsoleSessionId;
	if (dwConsoleSessionId == 0xFFFFFFFF) 
	{
		LOG(INFO) << "No console session active.";
		return E_FAIL;
	}

	// set session
	if (!SetTokenInformation(hPrimaryToken, TokenSessionId, &dwConsoleSessionId, sizeof(DWORD)))
	{
		*error = GetLastError();
		LOG(WARNING) << "SetTokenInformation failed.";
		return E_FAIL;
	}

	// get groups
	GetTokenInformation(hPrimaryToken, TokenGroups, NULL, 0, &dwLength);
	TOKEN_GROUPS *tgGroups = (TOKEN_GROUPS *)malloc(dwLength);
	if (!GetTokenInformation(hPrimaryToken, TokenGroups, tgGroups, dwLength, &dwLength))
	{
		*error = GetLastError();
		LOG(WARNING) << "GetTokenInformation failed.";
		return E_FAIL;
	}

	LOG(INFO) << "Group count: " << std::dec << tgGroups->GroupCount;
	for (size_t i = 0; i < tgGroups->GroupCount; i++)
	{
		SID_AND_ATTRIBUTES saaGroup = tgGroups->Groups[i];
		LPTSTR sSid;
		ConvertSidToStringSid(saaGroup.Sid, &sSid);
		LOG(INFO) << "Group membership: " << sSid << " Attributes: " << std::hex << saaGroup.Attributes;
		LocalFree(sSid);
	}


	//// set token groups
	//if (!SetTokenInformation(hPrimaryToken, TokenGroups, tgGroups, dwLength))
	//{
	//	*error = GetLastError();
	//	LOG(WARNING) << "SetTokenInformation failed.";
	//	return E_FAIL;
	//}

	// start process
	LOG(INFO) << "Starting process with command line: " << sCommandLine;

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.hStdOutput = mStdoutWrite;
	startupInfo.hStdError = mStderrWrite;
	startupInfo.hStdInput = mStdinRead;
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.wShowWindow = SW_HIDE;
	startupInfo.dwFlags |= STARTF_USESHOWWINDOW;
	startupInfo.lpDesktop = L"WinSta0\\Default";

	PROCESS_INFORMATION processInformation;
	BOOL status = CreateProcessAsUser(
		hPrimaryToken,
		NULL,
		sCommandLine,
		NULL,
		NULL,
		TRUE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&startupInfo,
		&processInformation);

	if (!status)
	{
		// start failed: return error
		*success = FALSE;
		*error = GetLastError();
		LOG(INFO) << "Starting process failed with error 0x" << std::hex << *error;
		return S_OK;
	}

	// start succeeded
	processStarted = true;
	mProcess = processInformation.hProcess;
	hThread = processInformation.hThread;
	dwProcessId = processInformation.dwProcessId;
	dwThreadId = processInformation.dwThreadId;
	LOG(INFO) << "Started process with id " << dwProcessId;

	// setup stdin pipe writer
	mStdinWriter = std::make_unique<PipeWriter>(mStdinWrite);

	*success = TRUE;
	*error = 0;

	return S_OK;
}

/*
STDMETHODIMP CExec::StartProcess(BSTR commandLine, BYTE *success, LONGLONG* error)
{
	_bstr_t sCommandLine(commandLine, true);

	// create pipes for stdin, stdout, stderr
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&mStdoutRead, &mStdoutWrite, &saAttr, 0))
		return E_FAIL;
	if (!SetHandleInformation(mStdoutRead, HANDLE_FLAG_INHERIT, 0))
		return E_FAIL;

	if (!CreatePipe(&mStderrRead, &mStderrWrite, &saAttr, 0))
		return E_FAIL;
	if (!SetHandleInformation(mStderrRead, HANDLE_FLAG_INHERIT, 0))
		return E_FAIL;

	if (!CreatePipe(&mStdinRead, &mStdinWrite, &saAttr, 0))
		return E_FAIL;
	if (!SetHandleInformation(mStdinWrite, HANDLE_FLAG_INHERIT, 0))
		return E_FAIL;

	// start process
	LOG(INFO) << "Starting process with command line: " << sCommandLine;
	
	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.hStdOutput = mStdoutWrite;
	startupInfo.hStdError = mStderrWrite;
	startupInfo.hStdInput = mStdinRead;
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.wShowWindow = SW_HIDE;
	startupInfo.dwFlags |= STARTF_USESHOWWINDOW;

	PROCESS_INFORMATION processInformation;
	BOOL status = CreateProcess(
		NULL,
		sCommandLine,
		NULL,
		NULL,
		TRUE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&startupInfo,
		&processInformation);

	if (!status)
	{
		// start failed: return error
		*success = FALSE;
		*error = GetLastError();
		LOG(INFO) << "Starting process failed with error 0x" << std::hex << *error;
		return S_OK;
	}

	// start succeeded
	processStarted = true;
	mProcess = processInformation.hProcess;
	hThread = processInformation.hThread;
	dwProcessId = processInformation.dwProcessId;
	dwThreadId = processInformation.dwThreadId;
	LOG(INFO) << "Started process with id " << dwProcessId;

	//CloseHandle(mStdoutWrite);

	// setup stdin pipe writer
	mStdinWriter = std::make_unique<PipeWriter>(mStdinWrite);

	*success = TRUE;
	*error = 0;

	//Sleep(10000);

	return S_OK;
}
*/

STDMETHODIMP CExec::ReadStdout(BSTR* data, long *dataLength)
{
	auto buf = ReadFromPipe(mStdoutRead);
	*data = VectorToBstr(buf);
	*dataLength = (long)buf.size();
	return S_OK;
}

STDMETHODIMP CExec::ReadStderr(BSTR* data, long *dataLength)
{
	auto buf = ReadFromPipe(mStderrRead);
	*data = VectorToBstr(buf);
	*dataLength = (long)buf.size();
	return S_OK;
}

STDMETHODIMP CExec::WriteStdin(BSTR data, long dataLength)
{
	auto vData = BstrToVector<char>(data, dataLength);
	mStdinWriter->Write(vData);
	return S_OK;
}

STDMETHODIMP CExec::GetTerminationStatus(BYTE* hasTerminated, LONGLONG* exitCode)
{
	DWORD status = WaitForSingleObject(mProcess, 0);
	if (status == WAIT_OBJECT_0)
	{
		// process has terminated
		*hasTerminated = TRUE;
		DWORD dwExitCode;
		if (!GetExitCodeProcess(mProcess, &dwExitCode))
			return E_FAIL;
		*exitCode = dwExitCode;

		LOG(INFO) << "Process has terminated with exit code " << dwExitCode;
		return S_OK;
	}
	else if (status == WAIT_TIMEOUT)
	{
		// process is still running
		*hasTerminated = FALSE;
		*exitCode = 0;
		return S_OK;
	}
	else
		return E_FAIL;
}

// do nothing handler for console events
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	return TRUE;
}

STDMETHODIMP CExec::SendControl(ControlEvent evt)
{
	// we must attach to the process' console to send the event
	if (!AttachConsole(dwProcessId))
	{
		LOG(WARNING) << "Attach to process " << dwProcessId << " console failed";
		return S_OK;
	}

	// register our own handler so that we do not get terminated ourselves
	SetConsoleCtrlHandler(&CtrlHandler, TRUE);

	// send event	
	switch (evt)
	{
	case CONTROLEVENT_C:
		LOG(INFO) << "Sending CTRL+C event";
		GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
		break;
	case CONTROLEVENT_BREAK:
		LOG(INFO) << "Sending CTRL+BREAK event";
		GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
		break;
	default:
		LOG(ERROR) << "Unknown event to send specified";
	}

	// detach from process' console
	if (!FreeConsole())
		LOG(WARNING) << "Detach from console failed";

	return S_OK;
}

STDMETHODIMP CExec::Ping()
{
	mLastPingTime = clock();
	return S_OK;
}

void CExec::KillProcess()
{
	if (!processStarted || mProcessKilled)
		return;

	LOG(INFO) << "Terminating process " << dwProcessId;
	if (!TerminateProcess(mProcess, 0))
		LOG(WARNING) << "Process termination failed";

	mProcessKilled = true;
}