// Exec.cpp : Implementation of CExec

#include "stdafx.h"
#include "Exec.h"


_bstr_t VectorToBstr(const std::vector<char> &vec)
{
	BSTR bstr = SysAllocStringByteLen(vec.data(), (UINT)vec.size());
	return _bstr_t(bstr, false);
}

std::vector<char> BstrToVector(const _bstr_t &bstr)
{
	unsigned int len = bstr.length();
	std::vector<char> vec(len);
	// casting to non-const is ok, because we just copy the data from the BSTR
	memcpy(vec.data(), ((_bstr_t)bstr).GetBSTR(), len);
	return vec;
}

std::vector<char> ReadFromPipe(HANDLE hPipe, const size_t maxLength = 10000)
{
	std::vector<char> empty;

	DWORD bytesAvail;
	if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL))
		return empty;
	if (bytesAvail == 0)
		return empty;

	std::vector<char> buffer(min(bytesAvail, maxLength));
	DWORD bytesRead;
	if (!ReadFile(hPipe, buffer.data(), (DWORD)buffer.size(), &bytesRead, NULL))
		return empty;
	buffer.resize(bytesRead);

	return buffer;
}



// CExec

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

	PROCESS_INFORMATION processInformation;
	BOOL status = CreateProcess(
		NULL,
		sCommandLine,
		NULL,
		NULL,
		FALSE,
		0,
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

STDMETHODIMP CExec::ReadStdout(BSTR* data)
{
	*data = VectorToBstr(ReadFromPipe(mStdoutRead));
	return S_OK;
}

STDMETHODIMP CExec::ReadStderr(BSTR* data)
{
	*data = VectorToBstr(ReadFromPipe(mStderrRead));
	return S_OK;
}

STDMETHODIMP CExec::WriteStdin(BSTR data)
{
	std::vector<char> vData = BstrToVector(_bstr_t(data, true));
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


