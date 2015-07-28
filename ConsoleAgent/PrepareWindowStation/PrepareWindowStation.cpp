
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <sddl.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <stdexcept>

#import <ConsoleServer.tlb>
#include "utils.h"
#include "ntapi.h"

using namespace std;

INITIALIZE_EASYLOGGINGPP;


ConsoleServerLib::IPrepareWindowStationCommPtr pComm;


static ATL::CSecurityDesc GetObjectSecurity(HANDLE handle)
{
	SECURITY_INFORMATION siRequested = DACL_SECURITY_INFORMATION;
	PSECURITY_DESCRIPTOR psd = NULL;
	DWORD dwSize = 0;
	DWORD dwSizeNeeded;

	if (NtQuerySecurityObject(handle, siRequested, psd, dwSize, &dwSizeNeeded) == STATUS_BUFFER_TOO_SMALL)
	{
		psd = (PSECURITY_DESCRIPTOR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSizeNeeded);
		if (psd == NULL)
			throw runtime_error("Could not allocate memory for security descriptor.");
		dwSize = dwSizeNeeded;

		if (NtQuerySecurityObject(handle, siRequested, psd, dwSize, &dwSizeNeeded) != STATUS_SUCCESS)
			throw runtime_error("Could not read kernel object security.");
	}
	else
		throw runtime_error("Could not read kernel object security size.");

	ATL::CSecurityDesc csd(*reinterpret_cast<SECURITY_DESCRIPTOR *>(psd));
	HeapFree(GetProcessHeap(), 0, psd);
	return csd;
}


static void SetObjectSecurity(HANDLE handle, const ATL::CSecurityDesc &csd)
{
	SECURITY_INFORMATION siRequested = DACL_SECURITY_INFORMATION;
	if (NtSetSecurityObject(handle, siRequested, (PSECURITY_DESCRIPTOR)csd.GetPSECURITY_DESCRIPTOR()) != STATUS_SUCCESS)
		throw runtime_error("Could not set kernel object security.");
}


void GrantAccessOnHandle(HANDLE handle, CSid sid, ACCESS_MASK accessMask)
{
	CSecurityDesc csd = GetHandleSecurity(handle);

	CDacl dacl;
	if (!csd.GetDacl(&dacl))
		throw runtime_error("Could not read ACL.");
	dacl.AddAllowedAce(sid, accessMask);
	csd.SetDacl(dacl);

	SetHandleSecurity(handle, csd);
}


void GrantAccessOnObject(HANDLE handle, CSid sid, ACCESS_MASK accessMask)
{
	CSecurityDesc csd = GetObjectSecurity(handle);

	CDacl dacl;
	if (!csd.GetDacl(&dacl))
		throw runtime_error("Could not read ACL.");
	dacl.AddAllowedAce(sid, accessMask);
	csd.SetDacl(dacl);

	SetObjectSecurity(handle, csd);
}


void RevokeAccessOnHandle(HANDLE handle, CSid sid)
{
	CSecurityDesc csd = GetHandleSecurity(handle);

	CDacl dacl;
	if (!csd.GetDacl(&dacl))
		throw runtime_error("Could not read ACL.");
	dacl.RemoveAces(sid);
	csd.SetDacl(dacl);

	SetHandleSecurity(handle, csd);
}


void RevokeAccessOnObject(HANDLE handle, CSid sid)
{
	CSecurityDesc csd = GetObjectSecurity(handle);

	CDacl dacl;
	if (!csd.GetDacl(&dacl))
		throw runtime_error("Could not read ACL.");
	dacl.RemoveAces(sid);
	csd.SetDacl(dacl);

	SetObjectSecurity(handle, csd);
}


bool HasSidAceOnHandle(HANDLE handle, CSid sid)
{
	CSecurityDesc csd = GetHandleSecurity(handle);

	CDacl dacl;
	if (!csd.GetDacl(&dacl))
		throw runtime_error("Could not read ACL.");
	
	for (unsigned int i = 0; i < dacl.GetAceCount(); i++)
	{
		CSid aceSid;
		dacl.GetAclEntry(i, &aceSid);
		if (aceSid == sid)
			return true;
	}
	return false;
}


bool HasSidAceOnObject(HANDLE handle, CSid sid)
{
	CSecurityDesc csd = GetObjectSecurity(handle);

	CDacl dacl;
	if (!csd.GetDacl(&dacl))
		throw runtime_error("Could not read ACL.");

	for (unsigned int i = 0; i < dacl.GetAceCount(); i++)
	{
		CSid aceSid;
		dacl.GetAclEntry(i, &aceSid);
		if (aceSid == sid)
			return true;
	}
	return false;
}


HANDLE OpenDirectoryObject(const wstring path)
{
	UNICODE_STRING usPath;
	if (RtlUnicodeStringInit(&usPath, path.c_str()) != STATUS_SUCCESS)
		throw runtime_error("RtlUnicodeStringInit failed");

	OBJECT_ATTRIBUTES objAttrs;
	HANDLE hDir;
	InitializeObjectAttributes(&objAttrs, &usPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
	if (NtOpenDirectoryObject(&hDir, STANDARD_RIGHTS_READ | READ_CONTROL | WRITE_DAC, &objAttrs) != STATUS_SUCCESS)
		throw runtime_error("NtOpenDirectoryObject failed");
	return hDir;
}



void PrepareDesktop(wstring desktopName, CSid clientSid, 
					HWINSTA &hWinsta, HDESK &hDesk, bool &hadWinstaAce)
{
	// obtain a handle to the interactive windowstation
	hWinsta = OpenWindowStation(L"winsta0", FALSE, READ_CONTROL | WRITE_DAC);
	if (hWinsta == NULL)
		throw runtime_error("Could not open window station.");

	// check if user already has some ACE on window station
	hadWinstaAce = HasSidAceOnHandle(hWinsta, clientSid);

	// add the user to interactive windowstation
	LOG(INFO) << "PrepareWindowStation: Grant Winsta0 access to " << SidToString(clientSid);
	GrantAccessOnHandle(hWinsta, clientSid, WINSTA_ALL_ACCESS);
	GrantAccessOnHandle(hWinsta, clientSid, GENERIC_ALL);

	// try to open desktop
	hDesk = OpenDesktop(desktopName.c_str(), 0, FALSE, GENERIC_ALL);
	if (hDesk == NULL)
	{
		// if desktop does not exist, create it 

		// create and obtain a handle to the specfiied
		hDesk = CreateDesktop(desktopName.c_str(), NULL, NULL, 0, GENERIC_ALL, NULL);
		if (hDesk == NULL)
			throw runtime_error("Could not create and open desktop.");
	}

	// add user to desktop
	LOG(INFO) << "PrepareWindowStation: Grant desktop " << desktopName << " access to " << SidToString(clientSid);
	GrantAccessOnHandle(hDesk, clientSid, GENERIC_ALL);
}

DWORD GetOurSessionId()
{
	DWORD processId = GetCurrentProcessId();
	DWORD sessionId;
	if (!ProcessIdToSessionId(processId, &sessionId))
		throw runtime_error("ProcessIdToSessionId failed");
	return sessionId;
}

void PrepareObjectDirectory(CSid clientSid, HANDLE &hObjDirectory, bool &hadObjAce)
{
	// build object directory path
	wstringstream objNameStream;
	objNameStream << L"\\Sessions\\" << dec << GetOurSessionId() << "\\BaseNamedObjects";
	wstring objName = objNameStream.str();

	// obtain handle to object directory
	hObjDirectory = OpenDirectoryObject(objName);

	// check if user already has some ACE on object directory
	hadObjAce = HasSidAceOnObject(hObjDirectory, clientSid);

	// allow user to create objects in object directory
	LOG(INFO) << "PrepareWindowStation: Grant " << objName << " access to " << SidToString(clientSid);
	GrantAccessOnObject(hObjDirectory, clientSid, 
						DIRECTORY_CREATE_OBJECT | DIRECTORY_CREATE_SUBDIRECTORY |
						DIRECTORY_QUERY | DIRECTORY_TRAVERSE | READ_CONTROL | GENERIC_READ);
}


void UnprepareDesktop(wstring desktopName, CSid clientSid, HWINSTA hWinsta, HDESK hDesk, bool hadWinstaAce)
{
	// revoke user access to desktop
	LOG(INFO) << "PrepareWindowStation: Revoke desktop " << desktopName << " access from " << SidToString(clientSid);
	RevokeAccessOnHandle(hDesk, clientSid);

	// revoke user access to window station
	if (!hadWinstaAce)
	{
		LOG(INFO) << "PrepareWindowStation: Revoke Winsta0 access from " << SidToString(clientSid);
		RevokeAccessOnHandle(hWinsta, clientSid);
	}

	// close handles
	CloseDesktop(hDesk);
	CloseWindowStation(hWinsta);
}


void UnprepareObjectDirectory(CSid clientSid, HANDLE hObjDirectory, bool hadObjAce)
{
	// reove user access to object directory
	if (!hadObjAce)
	{
		LOG(INFO) << "PrepareWindowStation: Revoke object directory access from " << SidToString(clientSid);
		RevokeAccessOnObject(hObjDirectory, clientSid);
	}

	// close handle
	NtClose(hObjDirectory);
}


// do nothing handler for console events
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	return TRUE;
}

void ExecuteConsoleEventOrders()
{
	BYTE hasOrder;
	DWORD processId;
	ConsoleServerLib::ControlEvent evt;

	pComm->GetConsoleEventOrder(&hasOrder, &processId, &evt);

	if (hasOrder)
	{
		LOG(INFO) << "PrepareWindowStation.exe: Sending control event to process " << dec << processId;

		// we must attach to the process' console to send the event
		if (!AttachConsole(processId))
		{
			LOG(WARNING) << "PrepareWindowStation.exe: Attach to process " << processId << " console failed " << GetLastError();
			return;
		}

		// register our own handler so that we do not get terminated ourselves
		if (!SetConsoleCtrlHandler(&CtrlHandler, TRUE))
		{
			LOG(ERROR) << "PrepareWindowStation.exe: Could not register console control event handler.";
			return;
		}
		
		// send event	
		switch (evt)
		{
		case ConsoleServerLib::CONTROLEVENT_C:
			LOG(INFO) << "PrepareWindowStation.exe: Sending CTRL+C event";
			GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
			break;
		case ConsoleServerLib::CONTROLEVENT_BREAK:
			LOG(INFO) << "PrepareWindowStation.exe: Sending CTRL+BREAK event";
			GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
			break;
		default:
			LOG(ERROR) << "PrepareWindowStation.exe: Unknown event to send specified";
		}

		// detach from process' console
		if (!FreeConsole())
			LOG(WARNING) << "PrepareWindowStation.exe: Detach from console failed";
	}

}

void WaitWhileActive()
{
	try
	{
		BYTE active = TRUE;
		while (active)
		{
			ExecuteConsoleEventOrders();

			pComm->IsActive(&active);
			this_thread::sleep_for(chrono::milliseconds(100));
		}		
	}
	catch (_com_error err)
	{
		// if communication with server fails, we assume that session is not active anymore
	}
}



extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
								LPTSTR lpCmdLine, int nShowCmd)
{
	int argc;
	_TCHAR **argv = CommandLineToArgvW(lpCmdLine, &argc);

	// initialize COM
	CoInitialize(NULL);

	// configure logging
	ConfigureLogging(true);

	// handle command line arguments
	if (argc != 1)
	{
		std::wcerr << "Usage: PrepareWindowStation.exe <PreparationID>" << std::endl;
		return 1;
	}
	wstring preparationId(argv[0]);

	try
	{
		// connect to COM server
		pComm.CreateInstance(__uuidof(ConsoleServerLib::PrepareWindowStationComm));
		pComm->Attach(WStringToBStr(preparationId));

		// get desktop name and client sid
		BSTR bsDesktopName;
		BSTR bsClientSidString;
		pComm->GetData(&bsDesktopName, &bsClientSidString);
		wstring desktopName = BStrToWString(bsDesktopName);
		wstring clientSidString = BStrToWString(bsClientSidString);
		CSid clientSid = ParseSid(clientSidString);

		// prepare desktop
		HWINSTA hWinsta;
		HDESK hDesk;
		bool hadWinstaAce;
		PrepareDesktop(desktopName, clientSid, hWinsta, hDesk, hadWinstaAce);

		// prepare object directory
		HANDLE hObjDirectory;
		bool hadObjDirectoryAce;
		PrepareObjectDirectory(clientSid, hObjDirectory, hadObjDirectoryAce);

		// signal that desktop is ready
		pComm->PreparationCompleted();

		// wait while session is active to keep desktop from being deleted
		WaitWhileActive();

		// unprepare object directory
		UnprepareObjectDirectory(clientSid, hObjDirectory, hadObjDirectoryAce);

		// unprepare desktop
		UnprepareDesktop(desktopName, clientSid, hWinsta, hDesk, hadWinstaAce);
	}
	catch (runtime_error err)
	{
		LOG(ERROR) << "PrepareWindowStation: " << err.what();
		return -1;
	}
	catch (_com_error err)
	{
		LOG(ERROR) << "PrepareWindowStation: COM error: " << err.ErrorMessage();
		return -2;
	}

	return 0;
}

