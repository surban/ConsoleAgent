#include <stdexcept>

#include "ntapi.h"

using namespace std;

// ntdll.dll functions
NtOpenDirectoryObject_t NtOpenDirectoryObject;
NtQuerySecurityObject_t NtQuerySecurityObject;
NtSetSecurityObject_t NtSetSecurityObject;
NtClose_t NtClose;


NTSTATUS RtlUnicodeStringInit(OUT PUNICODE_STRING  DestinationString,
							  IN  PCWSTR		   pszSrc)
{
	DestinationString->Buffer = (PWCH)pszSrc;
	DestinationString->Length = (USHORT)wcslen(pszSrc) * 2;
	DestinationString->MaximumLength = DestinationString->Length;
	return STATUS_SUCCESS;
}


void ImportFunctions()
{
	HMODULE hNtdll = LoadLibrary(L"ntdll.dll");

	NtOpenDirectoryObject = (NtOpenDirectoryObject_t)GetProcAddress(hNtdll, "NtOpenDirectoryObject");
	if (NtOpenDirectoryObject == NULL)
		throw runtime_error("Symbol NtOpenDirectoryObject not found");

	NtQuerySecurityObject = (NtQuerySecurityObject_t)GetProcAddress(hNtdll, "NtQuerySecurityObject");
	if (NtQuerySecurityObject == NULL)
		throw runtime_error("Symbol NtQuerySecurityObject not found");

	NtSetSecurityObject = (NtSetSecurityObject_t)GetProcAddress(hNtdll, "NtSetSecurityObject");
	if (NtSetSecurityObject == NULL)
		throw runtime_error("Symbol NtSetSecurityObject not found");

	NtClose = (NtClose_t)GetProcAddress(hNtdll, "NtClose");
	if (NtClose == NULL)
		throw runtime_error("Symbol NtClose not found");
}



