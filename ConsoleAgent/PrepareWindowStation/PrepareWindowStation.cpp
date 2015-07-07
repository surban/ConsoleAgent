
#include "stdafx.h"

#include <iostream>
#include <thread>
#include <chrono>

// permissions to set
#define WINSTA_ALL (WINSTA_ACCESSCLIPBOARD  | WINSTA_ACCESSGLOBALATOMS | \
					WINSTA_CREATEDESKTOP | WINSTA_ENUMDESKTOPS | \
					WINSTA_ENUMERATE | WINSTA_EXITWINDOWS | \
				    WINSTA_READATTRIBUTES | WINSTA_READSCREEN | \
					WINSTA_WRITEATTRIBUTES | DELETE | \
					READ_CONTROL | WRITE_DAC | \
					WRITE_OWNER)

#define DESKTOP_ALL (DESKTOP_CREATEMENU      | DESKTOP_CREATEWINDOW  | \
					 DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL | \
					 DESKTOP_JOURNALPLAYBACK | DESKTOP_JOURNALRECORD | \
					 DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | \
					 DESKTOP_WRITEOBJECTS | DELETE | \
					 READ_CONTROL | WRITE_DAC | \
					 WRITE_OWNER)

#define GENERIC_ACCESS (GENERIC_READ    | GENERIC_WRITE | \
						GENERIC_EXECUTE | GENERIC_ALL)


// gives full control of the specified window station to the given SID
BOOL AddTheAceWindowStation(
	HWINSTA hwinsta,         // handle to a windowstation
	PSID    psid             // logon sid of the process
);

// gives full control of the specified desktop to the given SID
BOOL AddTheAceDesktop(
	HDESK hdesk,             // handle to a desktop
	PSID  psid               // logon sid of the process
);



int _tmain(int argc, _TCHAR* argv[])
{
	// handle command line arguments
	if (argc != 4)
	{
		std::wcerr << "Usage: " << argv[0] << " <SID> <DesktopName> <Pipe>" << std::endl;
		return 1;
	}
	LPCTSTR psSid = argv[1];
	LPCTSTR psDesktopName = argv[2];
	LPCTSTR psPipeName = argv[3];

	// parse sid
	PSID psid;
	if (!ConvertStringSidToSid(psSid, &psid))
	{
		std::wcerr << "Could not parse SID." << std::endl;
		return 2;
	}
	
	// obtain a handle to the interactive windowstation
	HWINSTA hwinsta = OpenWindowStation(L"winsta0", FALSE, READ_CONTROL | WRITE_DAC);
	if (hwinsta == NULL)
	{
		std::wcerr << "Could not open window station." << std::endl;
		return 3;
	}	

	// try to open desktop
	HDESK hdesk = OpenDesktop(psDesktopName, 0, FALSE, GENERIC_ALL);
	if (hdesk == NULL)
	{
		// if desktop does not exist, create it and set access permissions accordingly

		// create and obtain a handle to the specfiied
		HDESK hdesk = CreateDesktop(psDesktopName, NULL, NULL, 0, GENERIC_ALL, NULL);
		if (hdesk == NULL)
		{
			std::wcerr << "Could not create and open desktop." << std::endl;
			return 4;
		}

		// add the user to interactive windowstation
		if (!AddTheAceWindowStation(hwinsta, psid))
			return 5;

		// add user to desktop
		if (!AddTheAceDesktop(hdesk, psid))
			return 6;
	}

	std::wcout << "Desktop created." << std::endl;
	std::wcout << "Connecting to pipe " << psPipeName << std::endl;

	// connect to pipe to signal that desktop was created
	HANDLE hPipe = CreateNamedPipe(psPipeName, PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES, 100, 100, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		std::wcerr << "Pipe connection failed: 0x" << std::hex << GetLastError() << std::endl;
		while (true);
		return 7;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	std::wcout << "Waiting for termination." << std::endl;

	// wait for termination signal
	const size_t bufferSize = 10;
	char *buffer = new char[bufferSize];
	DWORD bytesRead;
	if (!ReadFile(hPipe, buffer, bufferSize, &bytesRead, NULL))
	{
		std::wcerr << "ReadFile on pipe failed: 0x" << std::hex << GetLastError() << std::endl;
		while (true);
		return 8;
	}

	// free the buffer for the logon sid
	LocalFree(psid);

	// close the handles to the interactive windowstation and desktop
	CloseWindowStation(hwinsta);
	CloseDesktop(hdesk);

	return 0;
}


BOOL AddTheAceWindowStation(HWINSTA hwinsta, PSID psid)
{
	ACCESS_ALLOWED_ACE   *pace = NULL;
	ACL_SIZE_INFORMATION aclSizeInfo;
	BOOL                 bDaclExist;
	BOOL                 bDaclPresent;
	BOOL                 bSuccess = FALSE; 
	DWORD                dwNewAclSize;
	DWORD                dwSidSize = 0;
	DWORD                dwSdSizeNeeded;
	PACL                 pacl;
	PACL                 pNewAcl = NULL;
	PSECURITY_DESCRIPTOR psd = NULL;
	PSECURITY_DESCRIPTOR psdNew = NULL;
	PVOID                pTempAce;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	unsigned int         i;

	__try
	{
		// obtain the dacl for the windowstation
		if (!GetUserObjectSecurity(hwinsta,	&si, psd, dwSidSize, &dwSdSizeNeeded))
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(),	HEAP_ZERO_MEMORY, dwSdSizeNeeded);
				if (psd == NULL)
					__leave;

				psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSdSizeNeeded);
				if (psdNew == NULL)
					__leave;

				dwSidSize = dwSdSizeNeeded;

				if (!GetUserObjectSecurity(hwinsta,	&si, psd, dwSidSize, &dwSdSizeNeeded))
					__leave;
			}
			else
				__leave;

		// create a new dacl
		if (!InitializeSecurityDescriptor(psdNew, SECURITY_DESCRIPTOR_REVISION))
			__leave;

		// get dacl from the security descriptor
		if (!GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl, &bDaclExist))
			__leave;

		// initialize
		ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
		aclSizeInfo.AclBytesInUse = sizeof(ACL);

		// call only if the dacl is not NULL
		if (pacl != NULL)
		{
			// get the file ACL size info
			if (!GetAclInformation(pacl, (LPVOID)&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation))
				__leave;
		}

		// compute the size of the new acl
		dwNewAclSize = aclSizeInfo.AclBytesInUse + (2 *	sizeof(ACCESS_ALLOWED_ACE)) + 
			(2 * GetLengthSid(psid)) - (2 *	sizeof(DWORD));

		// allocate memory for the new acl
		pNewAcl = (PACL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNewAclSize);
		if (pNewAcl == NULL)
			__leave;

		// initialize the new dacl
		if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
			__leave;

		// if DACL is present, copy it to a new DACL
		if (bDaclPresent) 
		{
			// copy the ACEs to our new ACL
			if (aclSizeInfo.AceCount)
			{
				for (i = 0; i < aclSizeInfo.AceCount; i++)
				{
					// get an ACE
					if (!GetAce(pacl, i, &pTempAce))
						__leave;

					// add the ACE to the new ACL
					if (!AddAce(pNewAcl, ACL_REVISION, MAXDWORD, pTempAce, ((PACE_HEADER)pTempAce)->AceSize))
						__leave;
				}
			}
		}

		// add the first ACE to the windowstation
		pace = (ACCESS_ALLOWED_ACE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 
			sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) - sizeof(DWORD));
		if (pace == NULL)
			__leave;

		pace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
		pace->Header.AceFlags = CONTAINER_INHERIT_ACE |	INHERIT_ONLY_ACE |	OBJECT_INHERIT_ACE;
		pace->Header.AceSize = (WORD)(sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) - sizeof(DWORD));
		pace->Mask = GENERIC_ACCESS;

		if (!CopySid(GetLengthSid(psid), &pace->SidStart, psid))
			__leave;

		if (!AddAce(pNewAcl, ACL_REVISION, MAXDWORD, (LPVOID)pace, pace->Header.AceSize))
			__leave;

		// add the second ACE to the windowstation
		pace->Header.AceFlags = NO_PROPAGATE_INHERIT_ACE;
		pace->Mask = WINSTA_ALL;

		if (!AddAce(pNewAcl, ACL_REVISION, MAXDWORD, (LPVOID)pace, pace->Header.AceSize))
			__leave;

		// set new dacl for the security descriptor
		if (!SetSecurityDescriptorDacl(psdNew, TRUE, pNewAcl, FALSE))
			__leave;

		// set the new security descriptor for the windowstation
		if (!SetUserObjectSecurity(hwinsta, &si, psdNew))
			__leave;

		// indicate success
		bSuccess = TRUE;
	}
	__finally
	{
		// free the allocated buffers
		if (pace != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pace);
		if (pNewAcl != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);
		if (psd != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psd);
		if (psdNew != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
	}

	return bSuccess;
}


BOOL AddTheAceDesktop(HDESK hdesk, PSID psid)
{
	ACL_SIZE_INFORMATION aclSizeInfo;
	BOOL                 bDaclExist;
	BOOL                 bDaclPresent;
	BOOL                 bSuccess = FALSE; 
	DWORD                dwNewAclSize;
	DWORD                dwSidSize = 0;
	DWORD                dwSdSizeNeeded;
	PACL                 pacl = NULL;
	PACL                 pNewAcl = NULL;
	PSECURITY_DESCRIPTOR psd = NULL;
	PSECURITY_DESCRIPTOR psdNew = NULL;
	PVOID                pTempAce;
	SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
	unsigned int         i;

	__try
	{
		// obtain the security descriptor for the desktop object
		if (!GetUserObjectSecurity(hdesk, &si, psd,	dwSidSize, &dwSdSizeNeeded))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSdSizeNeeded);
				if (psd == NULL)
					__leave;

				psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSdSizeNeeded);
				if (psdNew == NULL)
					__leave;

				dwSidSize = dwSdSizeNeeded;

				if (!GetUserObjectSecurity(hdesk, &si, psd, dwSidSize, &dwSdSizeNeeded))
					__leave;
			}
			else
				__leave;
		}

		// create a new security descriptor
		if (!InitializeSecurityDescriptor(psdNew, SECURITY_DESCRIPTOR_REVISION))
			__leave;

		// obtain the dacl from the security descriptor
		if (!GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl, &bDaclExist))
			__leave;

		// initialize
		ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
		aclSizeInfo.AclBytesInUse = sizeof(ACL);

		// call only if dacl already present
		if (pacl != NULL)
		{
			// determine the size of the ACL info
			if (!GetAclInformation(pacl, (LPVOID)&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation))
				__leave;
		}

		// compute the size of the new acl
		dwNewAclSize = aclSizeInfo.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) +	GetLengthSid(psid) - sizeof(DWORD);

		// allocate buffer for the new acl
		pNewAcl = (PACL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwNewAclSize);
		if (pNewAcl == NULL)
			__leave;

		// initialize the new acl
		if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
			__leave;

		// if DACL is present, copy it to a new DACL
		if (bDaclPresent) 
		{
			// copy the ACEs to our new ACL
			if (aclSizeInfo.AceCount)
			{
				for (i = 0; i < aclSizeInfo.AceCount; i++)
				{
					// get an ACE
					if (!GetAce(pacl, i, &pTempAce))
						__leave;

					// add the ACE to the new ACL
					if (!AddAce(pNewAcl, ACL_REVISION, MAXDWORD, pTempAce, ((PACE_HEADER)pTempAce)->AceSize))
						__leave;
				}
			}
		}

		// add ace to the dacl
		if (!AddAccessAllowedAce(pNewAcl, ACL_REVISION, DESKTOP_ALL, psid))
			__leave;

		// set new dacl to the new security descriptor
		if (!SetSecurityDescriptorDacl(psdNew, TRUE, pNewAcl, FALSE))
			__leave;

		// set the new security descriptor for the desktop object
		if (!SetUserObjectSecurity(hdesk, &si, psdNew))
			__leave;

		// indicate success
		bSuccess = TRUE;
	}
	__finally
	{
		// free buffers
		if (pNewAcl != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);
		if (psd != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psd);
		if (psdNew != NULL)
			HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
	}

	return bSuccess;
}


