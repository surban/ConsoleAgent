
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

using namespace std;

INITIALIZE_EASYLOGGINGPP;


ConsoleServerLib::IPrepareWindowStationCommPtr pComm;


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

void WaitWhileActive(wstring preparationId)
{
	try
	{
		BYTE active = TRUE;
		while (active)
		{
			pComm->IsActive(WStringToBStr(preparationId), &active);
			this_thread::sleep_for(chrono::seconds(1));
		}		
	}
	catch (_com_error err)
	{
		// if communication with server fail, we assume that session is not active anymore
	}
}


int _tmain(int argc, _TCHAR* argv[])
{
	// initialize COM
	CoInitialize(NULL);

	// configure logging
	ConfigureLogging();

	// handle command line arguments
	if (argc != 2)
	{
		std::wcerr << "Usage: " << argv[0] << " <PreparationID>" << std::endl;
		return 1;
	}
	wstring preparationId(argv[1]);

	try
	{
		// connect to COM server
		pComm.CreateInstance(__uuidof(ConsoleServerLib::PrepareWindowStationComm));

		// get desktop name and client sid
		BSTR bsDesktopName;
		BSTR bsClientSidString;
		pComm->GetData(WStringToBStr(preparationId), &bsDesktopName, &bsClientSidString);
		wstring desktopName = BStrToWString(bsDesktopName);
		wstring clientSidString = BStrToWString(bsClientSidString);
		CSid clientSid = ParseSid(clientSidString);

		// prepare desktop
		HWINSTA hWinsta;
		HDESK hDesk;
		bool hadWinstaAce;
		PrepareDesktop(desktopName, clientSid, hWinsta, hDesk, hadWinstaAce);

		// signal that desktop is ready
		pComm->PreparationCompleted(WStringToBStr(preparationId));

		// wait while session is active to keep desktop from being deleted
		WaitWhileActive(preparationId);

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

