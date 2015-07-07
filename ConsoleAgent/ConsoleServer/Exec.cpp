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

	CreateProcessDesktop();
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

	//// get groups
	//GetTokenInformation(hPrimaryToken, TokenGroups, NULL, 0, &dwLength);
	//TOKEN_GROUPS *tgGroups = (TOKEN_GROUPS *)malloc(dwLength);
	//if (!GetTokenInformation(hPrimaryToken, TokenGroups, tgGroups, dwLength, &dwLength))
	//{
	//	*error = GetLastError();
	//	LOG(WARNING) << "GetTokenInformation failed.";
	//	return E_FAIL;
	//}

	//LOG(INFO) << "Group count: " << std::dec << tgGroups->GroupCount;
	//for (size_t i = 0; i < tgGroups->GroupCount; i++)
	//{
	//	SID_AND_ATTRIBUTES saaGroup = tgGroups->Groups[i];
	//	LPTSTR sSid;
	//	ConvertSidToStringSid(saaGroup.Sid, &sSid);
	//	LOG(INFO) << "Group membership: " << sSid << " Attributes: " << std::hex << saaGroup.Attributes;
	//	LocalFree(sSid);
	//}


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
	//startupInfo.lpDesktop = L"WinSta0\\Default";
	startupInfo.lpDesktop = L"WinSta0\\abc";

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


wstring GenerateUuidString()
{
	UUID uuid;
	UuidCreate(&uuid);

	RPC_WSTR csUuid;
	UuidToString(&uuid, &csUuid);
	std::wstring wsUuid(reinterpret_cast<wchar_t *>(csUuid));
	RpcStringFree(&csUuid);

	return wsUuid;
}

bool GetConsoleAccessToken(CHandle &chConsoleAccessToken)
{
	DWORD dwConsoleSessionId = WTSGetActiveConsoleSessionId();
	if (dwConsoleSessionId == 0xFFFFFFFF)
		return false;

	HANDLE hToken;
	if (!WTSQueryUserToken(dwConsoleSessionId, &hToken))
		return false;

	chConsoleAccessToken = CHandle(hToken);
	return true;
}

void CExec::CreateProcessDesktop()
{
	LOG(INFO) << "Creating process desktop";


	// obtain own sid
	CAccessToken catOwnToken;
	catOwnToken.GetProcessToken(TOKEN_READ | TOKEN_QUERY);
	CSid csOwnSid;
	catOwnToken.GetUser(&csOwnSid);
	wstring wsOwnSid(csOwnSid.Sid());
	LOG(INFO) << "Own sid is " << wsOwnSid;

	// obtain client sid
	CAccessToken catClientToken;
	catClientToken.OpenCOMClientToken(TOKEN_READ | TOKEN_QUERY);
	CSid csClientSid;
	catClientToken.GetUser(&csClientSid);
	wstring wsClientSid(csClientSid.Sid());
	LOG(INFO) << "Client sid is " << wsClientSid;

	// generate desktop name
	mProcessDesktopName = L"ConsoleServer_" + wsClientSid;

	// get console access token
	CHandle chConsoleToken;
	if (!GetConsoleAccessToken(chConsoleToken))
	{
		LOG(ERROR) << "Cannot get console access token.";
		return;
	}

	// create ACLs for events
	CDacl cdEvent;
	cdEvent.AddAllowedAce(csClientSid, GENERIC_ALL, 0);
	cdEvent.AddAllowedAce(csOwnSid, GENERIC_ALL, 0);
	CSecurityDesc csdEvent;
	csdEvent.SetOwner(csOwnSid);
	csdEvent.SetDacl(cdEvent);
	CSecurityAttributes csaEvent(csdEvent);


	// create events
	wstring wssUuid = GenerateUuidString();

	wstring wssReadyEventName = L"Global\\PrepareWindowStation_Ready_" + wssUuid;
	HANDLE hReadyEvent = CreateEvent(&csaEvent, TRUE, FALSE, wssReadyEventName.c_str());
	
	wstring wssReleaseEventName = L"Global\\PrepareWindowStation_Release_" + wssUuid;
	HANDLE hReleaseEvent = CreateEvent(&csaEvent, TRUE, FALSE, wssReleaseEventName.c_str());

	// create named pipe
	LOG(INFO) << "Creating pipe.";
	wstring wssPipename = L"\\\\.\\pipe\\ConsoleServer_PrepareWindowStation_" + GenerateUuidString();

	CDacl cdPipe;
	cdPipe.AddAllowedAce(csClientSid, GENERIC_ALL, 0);
	cdPipe.AddAllowedAce(csOwnSid, GENERIC_ALL, 0);

	CSecurityDesc csdPipe;
	csdPipe.SetOwner(csOwnSid);
	//csdPipe.SetGroup(CSid());
	csdPipe.SetDacl(cdPipe);

	CSecurityAttributes csaPipe(csdPipe);

	HANDLE hPreparePipe = CreateNamedPipe(wssPipename.c_str(), PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT, PIPE_UNLIMITED_INSTANCES, 100, 100, 0, &csaPipe);
	if (hPreparePipe == INVALID_HANDLE_VALUE)
	{
		LOG(ERROR) << "Creation of PrepareWindowStation pipe failed.";
		return;
	}
	mPreparePipe = CHandle(hPreparePipe);
	LOG(INFO) << "Pipe created: " << wssPipename;


	// execute PrepareWindowStation on console session
	wstring wsCmdLine = L"PrepareWindowStation.exe " + 
		wsClientSid + L" " + mProcessDesktopName + L" " + wssPipename;

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);

	PROCESS_INFORMATION processInformation;
	BOOL status = CreateProcessAsUser(
		chConsoleToken,
		NULL,
		const_cast<LPWSTR>(wsCmdLine.c_str()),
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
		LOG(ERROR) << "CreateProcessAsUser failed for console user.";
		return;
	}

	CloseHandle(processInformation.hProcess);
	CloseHandle(processInformation.hThread);

	// wait for pipe connection that indicates that desktop is prepared
	auto startTime = chrono::steady_clock::now();
	const auto maxWaitTime = std::chrono::seconds(10);
	do
	{	
		if (chrono::steady_clock::now() - startTime > maxWaitTime)
		{
			LOG(ERROR) << "Timeout while waiting for pipe connection of PrepareWindowStation.";
			return;
		}
		this_thread::sleep_for(std::chrono::milliseconds(10));

		ConnectNamedPipe(mPreparePipe, NULL);
		LOG(ERROR) << "Last error: 0x" << std::hex << GetLastError();
	} while (GetLastError() != ERROR_PIPE_CONNECTED);

	LOG(INFO) << "Process desktop prepared.";
}

void CExec::ReleaseProcessDesktop()
{

}