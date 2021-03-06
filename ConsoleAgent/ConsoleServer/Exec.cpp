// Exec.cpp : Implementation of CExec

#include "stdafx.h"
#include "Exec.h"
#include "utils.h"
#include "Timers.h"

#include <thread>

// CExec

#define COM_FAIL(msg)  {LOG(ERROR) << msg; return E_FAIL;}

CExec::CExec()
{
	LOG(INFO) << "CExec instantiated on thread " << std::this_thread::get_id();

	mProcessStarted = false;
	mProcessKilled = false;

	Ping();
	RegisterTimer(this, std::bind(&CExec::TimerCallback, this));
}

CExec::~CExec()
{
	KillProcess();
	DeregisterTimer(this);

	LOG(INFO) << "CExec destructed";
}

void CExec::TimerCallback()
{
	// check if client is alive
	if ((chrono::steady_clock::now() - mLastPingTime) > chrono::seconds(3) && !mProcessKilled)
	{
		LOG(WARNING) << "client did not ping -- terminating process";
		KillProcess();
	}
}


void CreateInheritablePipe(shared_ptr<CHandle> &readHandle, shared_ptr<CHandle> &writeHandle,
						   bool inheritRead, bool inheritWrite)
{
	SECURITY_ATTRIBUTES secAttrs;
	secAttrs.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAttrs.bInheritHandle = TRUE;
	secAttrs.lpSecurityDescriptor = NULL;

	HANDLE hRead;
	HANDLE hWrite;
	if (!CreatePipe(&hRead, &hWrite, &secAttrs, 0))
		throw runtime_error("Pipe creation failed");

	if (!inheritRead)
	{
		if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0))
			throw runtime_error("Setting pipe handle information failed");
	}

	if (!inheritWrite)
	{
		if (!SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0))
			throw runtime_error("Setting pipe handle information failed");
	}

	readHandle = make_shared<CHandle>(hRead);
	writeHandle = make_shared<CHandle>(hWrite);
}


wstring GetKnownFolderPath(REFKNOWNFOLDERID folderId, const CAccessToken &token)
{
	PWSTR path;
	if (SHGetKnownFolderPath(folderId, KF_FLAG_DONT_VERIFY, token.GetHandle(), &path) != S_OK)
		throw runtime_error("SHGetKnownFolderPath failed");
	wstring wsPath(path);
	CoTaskMemFree(path);
	return wsPath;
}

wstring GetSystemPath()
{
	const size_t bufSize = 255;
	wchar_t buffer[bufSize];
	if (GetSystemDirectory(buffer, bufSize) == 0)
		throw runtime_error("GetSystemDirectory failed");
	return wstring(buffer);
}

void CExec::DoStartProcess(wstring commandLine, wstring workingDir, wchar_t *environment, bool &success, LONGLONG &error)
{
	// check that window station is prepared
	if (!(mWindowStationPreparation && mWindowStationPreparation->IsPrepared()))
		throw runtime_error("Window station is not prepared.");

	// create job object
	HANDLE hJob = CreateJobObject(NULL, NULL);
	if (hJob == NULL)
		throw runtime_error("Could not create job object.");
	mJobHandle = CHandle(hJob);

	// create pipe for stdout
	shared_ptr<CHandle> stdoutReadHandle, stdoutWriteHandle;
	CreateInheritablePipe(stdoutReadHandle, stdoutWriteHandle, false, true);
	mStdoutReader = make_shared<PipeReader>(stdoutReadHandle, INPUTDETECT_PEEK, false);

	// create pipe for stderr
	shared_ptr<CHandle> stderrReadHandle, stderrWriteHandle;
	CreateInheritablePipe(stderrReadHandle, stderrWriteHandle, false, true);
	mStderrReader = make_shared<PipeReader>(stderrReadHandle, INPUTDETECT_PEEK, false);

	// create pipe for stdin
	shared_ptr<CHandle> stdinReadHandle, stdinWriteHandle;
	CreateInheritablePipe(stdinReadHandle, stdinWriteHandle, true, false);
	mStdinWriter = make_shared<PipeWriter>(stdinWriteHandle);

	// obtain primary client access token
	CAccessToken impersonationToken;
	if (!impersonationToken.OpenCOMClientToken(TOKEN_ALL_ACCESS))
		throw runtime_error("Could not open impersonation token.");
	CAccessToken clientToken;
	if (!impersonationToken.CreatePrimaryToken(&clientToken))
		throw runtime_error("Could not create primary token from impersonation token.");

	// get active console session
	DWORD consoleSessionId = WTSGetActiveConsoleSessionId();
	if (consoleSessionId == 0xFFFFFFFF)
		throw runtime_error("No console session active.");
	LOG(INFO) << "Console session id is " << std::dec << consoleSessionId;

	// set session on client access token
	if (!SetTokenInformation(clientToken.GetHandle(), TokenSessionId, &consoleSessionId, sizeof(DWORD)))
		throw runtime_error("Failed to set session on client token.");

	// start process
	LOG(INFO) << "Working directory: " << workingDir;
	LOG(INFO) << "Starting process with command line: " << commandLine;

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.hStdOutput = *stdoutWriteHandle;
	startupInfo.hStdError = *stderrWriteHandle;
	startupInfo.hStdInput = *stdinReadHandle;
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	//startupInfo.wShowWindow = SW_HIDE;
	//startupInfo.dwFlags |= STARTF_USESHOWWINDOW;
	wstring wsWinstaAndDesktop = L"WinSta0\\" + mWindowStationPreparation->GetDesktopName();
	startupInfo.lpDesktop = const_cast<LPWSTR>(wsWinstaAndDesktop.c_str());
	
	PROCESS_INFORMATION processInformation;
	bool isRetry = false;

	retryLaunch:
	BOOL status = CreateProcessAsUser(clientToken.GetHandle(), NULL, const_cast<LPWSTR>(commandLine.c_str()),
									  NULL, NULL, TRUE, CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, 
									  environment, workingDir.c_str(), &startupInfo, &processInformation);

	if (!status)
	{
		error = GetLastError();
		if (error == ERROR_DIRECTORY && !isRetry)
		{
			// Current directory is invalid. This may happen if user tries to launch OnConsole 
			// from a mapped network drive. Launch instead in Windows directory.
			workingDir = GetSystemPath();
			//workingDir = GetKnownFolderPath(FOLDERID_Documents, impersonationToken);
			LOG(WARNING) << "Changing working directory to " << workingDir;

			// inject warning message into stderr
			wstring msg = L"OnConsole: using working directory " + workingDir + L"\r\n";
			vector<wchar_t> vMsg;
			copy(msg.begin(), msg.end(), back_inserter(vMsg));
			mStdoutReader->InjectData(WcharToCharVector(vMsg));

			isRetry = true;
			goto retryLaunch;
		}
		else
		{
			// start failed: return error
			success = false;
			LOG(INFO) << "Starting process failed with error 0x" << std::hex << error;
			return;
		}
	}

	// start succeeded
	mProcessHandle = CHandle(processInformation.hProcess);
	mProcessId = processInformation.dwProcessId;
	CloseHandle(processInformation.hThread);
	mProcessStarted = true;

	// attach process to job
	if (!AssignProcessToJobObject(mJobHandle, mProcessHandle))
		throw runtime_error("Could not assign process to job object");

	LOG(INFO) << "Started process with process id " << std::dec << mProcessId;

	// report success
	success = true;
	error = 0;
}

STDMETHODIMP CExec::StartProcess(BSTR commandLine, BSTR workingDir, BSTR *environment, BYTE *success, LONGLONG *error)
{
	try
	{
		bool bSuccess;
		DoStartProcess(BStrToWString(commandLine), BStrToWString(workingDir), *environment, bSuccess, *error);
		*success = bSuccess;
		return S_OK;
	}
	catch (runtime_error err)
	{
		COM_FAIL(err.what());
	}
}

STDMETHODIMP CExec::ReadStdout(BSTR* data, long *dataLength)
{
	auto buf = mStdoutReader->Read();
	*data = VectorToBstr(buf);
	*dataLength = (long)buf.size();
	return S_OK;
}

STDMETHODIMP CExec::ReadStderr(BSTR* data, long *dataLength)
{
	auto buf = mStderrReader->Read();
	*data = VectorToBstr(buf);
	*dataLength = (long)buf.size();
	return S_OK;
}

STDMETHODIMP CExec::WriteStdin(BSTR* data, long dataLength)
{
	auto vData = BstrToVector<char>(*data, dataLength);
	mStdinWriter->Write(vData);
	return S_OK;
}

STDMETHODIMP CExec::GetTerminationStatus(BYTE* hasTerminated, LONGLONG* exitCode)
{
	DWORD status = WaitForSingleObject(mProcessHandle, 0);
	if (status == WAIT_OBJECT_0)
	{
		// process has terminated
		*hasTerminated = TRUE;
		DWORD dwExitCode;
		if (!GetExitCodeProcess(mProcessHandle, &dwExitCode))
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

STDMETHODIMP CExec::SendControl(ControlEvent evt)
{
	ConsoleEventOrder order;
	order.ProcessId = mProcessId;
	order.Event = evt;
	mWindowStationPreparation->OrderConsoleEvent(order);

	return S_OK;
}

STDMETHODIMP CExec::Ping()
{
	mLastPingTime = chrono::steady_clock::now();
	return S_OK;
}

void CExec::KillProcess()
{
	if (!mProcessStarted || mProcessKilled)
		return;

	LOG(INFO) << "Terminating job for process " << mProcessId;
	if (!TerminateJobObject(mJobHandle, 0))
		LOG(WARNING) << "Job termination failed";

	mProcessKilled = true;
}

STDMETHODIMP CExec::PrepareWindowStation(BYTE *success)
{
	try 
	{
		CAccessToken clientToken;
		if (!clientToken.OpenCOMClientToken(TOKEN_ALL_ACCESS))
			throw runtime_error("Could not open COM client token.");
		CSid clientSid;
		if (!clientToken.GetUser(&clientSid))
			throw runtime_error("Could not get COM client SID.");

		mWindowStationPreparation = WindowStationPreparation::Prepare(clientSid);
		*success = true;
	}
	catch (PreparationStillTerminating pst)
	{ 
		*success = false;
	}
	catch (runtime_error err)
	{
		COM_FAIL(err.what());
	}

	return S_OK;
}

STDMETHODIMP CExec::IsWindowStationPrepared(BYTE* prepared)
{
	if (mWindowStationPreparation)
	{
		*prepared = mWindowStationPreparation->IsPrepared();
		return S_OK;
	}
	else
	{
		*prepared = false;
		return S_OK;
	}
}
