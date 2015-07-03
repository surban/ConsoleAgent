#pragma once

#include <vector>
#include <comutil.h>

template<typename T>
static BSTR VectorToBstr(const std::vector<T> &vec)
{
	return SysAllocStringByteLen(vec.data(), (UINT)vec.size() * sizeof(T));
}

/*
template<typename T>
static std::vector<T> BstrToVector(const BSTR &bstr)
{
	UINT bytes = SysStringByteLen(bstr);
	return BstrToVector(bstr, bytes / sizeof(T));
}
*/

template<typename T>
static std::vector<T> BstrToVector(const BSTR &bstr, size_t elements)
{
	std::vector<T> vec(elements);
	CopyMemory(vec.data(), bstr, elements * sizeof(T));
	return vec;
}

std::string VectorToString(const std::vector<char> &vec)
{
	std::string s;
	for (int n = 0; n < vec.size(); n++)
	{
		s += std::string(1, vec[n]);
	}
	return s;
}

//void CharVectorToCOMArray(const std::vector<char> &vData, long &cDataLength, unsigned char *&cData)
//{
//	cData = (unsigned char *)midl_user_allocate(sizeof(unsigned char) * vData.size());
//	CopyMemory(cData, vData.data(), vData.size());
//	cDataLength = (long)vData.size();
//}
//
//std::vector<char> COMArrayToCharVector(long cDataLength, unsigned char *cData)
//{
//	std::vector<char> vData(cDataLength);
//	CopyMemory(vData.data(), cData, cDataLength);
//	return vData;
//}


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

