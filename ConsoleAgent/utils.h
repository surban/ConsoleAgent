#pragma once

#include <comutil.h>
#include <atlsecurity.h>

#include <string>
#include <vector>

#define ELPP_UNICODE
#include "easylogging++.h"


using namespace std;

template<typename T>
static BSTR VectorToBstr(const std::vector<T> &vec)
{
	return SysAllocStringByteLen(vec.data(), (UINT)vec.size() * sizeof(T));
}

template<typename T>
static std::vector<T> BstrToVector(const BSTR &bstr, size_t elements)
{
	std::vector<T> vec(elements);
	CopyMemory(vec.data(), bstr, elements * sizeof(T));
	return vec;
}

static std::string VectorToString(const std::vector<char> &vec)
{
	std::string s;
	for (int n = 0; n < vec.size(); n++)
	{
		s += std::string(1, vec[n]);
	}
	return s;
}


static wstring BStrToWString(BSTR bstr)
{
	return wstring(bstr);
}

static BSTR WStringToBStr(wstring wstr)
{
	return SysAllocString(wstr.c_str());
}

static std::vector<char> ReadFromPipe(HANDLE hPipe, const size_t maxLength = 10000)
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


static wstring SidToString(const ATL::CSid &sid)
{
	LPCTSTR sSid = sid.Sid(); 
	wstring wsSid(sSid);
	LocalFree(&sSid);
	return wsSid;
}

static ATL::CSid ParseSid(wstring sSid)
{
	PSID pSid;
	if (!ConvertStringSidToSid(sSid.c_str(), &pSid))
		throw runtime_error("Could not parse SID.");
	ATL::CSid sid((SID *)pSid);
	LocalFree(pSid);
	return sid;
}

static wstring GenerateUuidString()
{
	UUID uuid;
	UuidCreate(&uuid);

	RPC_WSTR csUuid;
	UuidToString(&uuid, &csUuid);
	std::wstring wsUuid(reinterpret_cast<wchar_t *>(csUuid));
	RpcStringFree(&csUuid);

	return wsUuid;
}


static void ConfigureLogging()
{
	el::Configurations logConf;
	logConf.setToDefault();
	logConf.set(el::Level::Global, el::ConfigurationType::Filename, "c:\\temp\\ConsoleServer.log");
	el::Loggers::reconfigureAllLoggers(logConf);
}


static ATL::CSecurityDesc GetHandleSecurity(HANDLE handle)
{
	SECURITY_INFORMATION siRequested = DACL_SECURITY_INFORMATION;
	PSECURITY_DESCRIPTOR psd = NULL;
	DWORD dwSize = 0;
	DWORD dwSizeNeeded;

	if (!GetUserObjectSecurity(handle, &siRequested, psd, dwSize, &dwSizeNeeded))
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSizeNeeded);
			if (psd == NULL)
				throw runtime_error("Could not allocate memory for security descriptor.");
			dwSize = dwSizeNeeded;

			if (!GetUserObjectSecurity(handle, &siRequested, psd, dwSize, &dwSizeNeeded))
				throw runtime_error("Could not read user object security.");
		}
		else
			throw runtime_error("Could not get user object security.");
	}

	ATL::CSecurityDesc csd(*reinterpret_cast<SECURITY_DESCRIPTOR *>(psd));
	HeapFree(GetProcessHeap(), 0, psd);
	return csd;
}

static void SetHandleSecurity(HANDLE handle, const ATL::CSecurityDesc &csd)
{
	SECURITY_INFORMATION siRequested = DACL_SECURITY_INFORMATION;
	if (!SetUserObjectSecurity(handle, &siRequested, (PSECURITY_DESCRIPTOR)csd.GetPSECURITY_DESCRIPTOR()))
		throw runtime_error("Could not set user object security.");
}


static wstring LuidToString(LUID luid)
{
	wstringstream wss;
	wss << hex << luid.HighPart << luid.LowPart;
	return wss.str();
}