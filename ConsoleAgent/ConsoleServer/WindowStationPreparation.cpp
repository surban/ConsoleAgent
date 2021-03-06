#include "stdafx.h"

#include "WindowStationPreparation.h"
#include "utils.h"

using namespace std;

map<wstring, weak_ptr<WindowStationPreparation>> WindowStationPreparation::sActivePrepationsById;
map<wstring, weak_ptr<WindowStationPreparation>> WindowStationPreparation::sActivePreparationsByClientAndConsole;
list<wstring> WindowStationPreparation::sRunningPrepareProcessesByClientAndConsole;

void GetConsoleAccessToken(CAccessToken &consoleToken)
{
	DWORD dwConsoleSessionId = WTSGetActiveConsoleSessionId();
	if (dwConsoleSessionId == 0xFFFFFFFF)
		throw runtime_error("Could not determine console session id.");

	HANDLE hToken;
	if (WTSQueryUserToken(dwConsoleSessionId, &hToken))
	{
		// connect to someones desktop session
		LOG(INFO) << "A user is logged onto the console session";
		consoleToken.Attach(hToken);
	}
	else
	{
		if (GetLastError() == ERROR_NO_TOKEN)
		{
			// nobody is logged onto the console session
			LOG(INFO) << "Nobody is logged onto the console session";

			// use own token with changed session id
			CAccessToken myToken;
			if (!myToken.GetProcessToken(TOKEN_ALL_ACCESS))
				throw runtime_error("Could not open process token");
			if (!myToken.CreatePrimaryToken(&consoleToken))
				throw runtime_error("Could not create primary token from process token");

			// set session id
			if (!SetTokenInformation(consoleToken.GetHandle(), TokenSessionId, &dwConsoleSessionId, sizeof(DWORD)))
			{
				LOG(ERROR) << "SetTokenInformation failed: 0x" << hex << GetLastError();
				throw runtime_error("Failed to set session on console access token.");
			}
		}
		else
		{
			LOG(ERROR) << "WTSQueryUserToken failed: 0x" << hex << GetLastError();
			throw runtime_error("Could not query console user token.");
		}
	}	
}

wstring ClientConsoleString(CSid clientSid, const CAccessToken &consoleToken)
{
	LUID sessionId;
	if (!consoleToken.GetLogonSessionId(&sessionId))
		throw runtime_error("Could not obtain logon session id.");
	return SidToString(clientSid) + L"_ON_" + LuidToString(sessionId);
}

shared_ptr<WindowStationPreparation> WindowStationPreparation::Prepare(CSid clientSid)
{
	CAccessToken consoleToken;
	GetConsoleAccessToken(consoleToken);
	
	// check if prepartion already exists for this client and console
	wstring ccStr = ClientConsoleString(clientSid, consoleToken);
	auto obj = sActivePreparationsByClientAndConsole[ccStr].lock();
	if (obj)
	{
		// reuse existing preparation
		LOG(INFO) << "Reusing WindowStationPreparation " << ccStr;
		return obj;
	}
	else
	{
		// check if a preparation process is still running, i.e. in the process of being terminated
		if (count(sRunningPrepareProcessesByClientAndConsole.begin(),
			      sRunningPrepareProcessesByClientAndConsole.end(),
			      ccStr) > 0)
		{
			LOG(INFO) << "WindowStationPrepartion " << ccStr << " is not active but still not terminated";
			throw PreparationStillTerminating();
		}

		// create new preparation
		LOG(INFO) << "Creating WindowStationPreparation " << ccStr;

		wstring preparationId = GenerateUuidString();
		auto obj = shared_ptr<WindowStationPreparation>(new WindowStationPreparation(preparationId, ccStr, clientSid));
	
		sActivePrepationsById[preparationId] = weak_ptr<WindowStationPreparation>(obj);
		sActivePreparationsByClientAndConsole[ccStr] = weak_ptr<WindowStationPreparation>(obj);

		obj->StartPreparation();
		return obj;
	}
}

void WindowStationPreparation::PrepareProcessStarted(wstring clientConsoleString)
{
	LOG(INFO) << "PrepareWindowStation.exe connected for " << clientConsoleString;
	sRunningPrepareProcessesByClientAndConsole.push_back(clientConsoleString);
}

void WindowStationPreparation::PrepareProcessTerminated(wstring clientConsoleString)
{
	LOG(INFO) << "PrepareWindowStation.exe disconnected for " << clientConsoleString;
	sRunningPrepareProcessesByClientAndConsole.remove(clientConsoleString);
}

shared_ptr<WindowStationPreparation> WindowStationPreparation::GetPreparationById(wstring preparationId)
{
	if (sActivePrepationsById.count(preparationId) == 0)
		return shared_ptr<WindowStationPreparation>();
	return sActivePrepationsById[preparationId].lock();
}

WindowStationPreparation::WindowStationPreparation(wstring preparationId, wstring clientConsoleString, CSid clientSid)
{
	mPreparationId = preparationId;
	mClientConsoleString = clientConsoleString;

	mClientSid = clientSid;
	mIsPrepared = false;
}

WindowStationPreparation::~WindowStationPreparation()
{
	LOG(INFO) << "Destructing WindowStationPreparation " << mClientConsoleString;

	sActivePrepationsById.erase(mPreparationId);
	sActivePreparationsByClientAndConsole.erase(mClientConsoleString);
}

CSid WindowStationPreparation::GetClientSid()
{
	return mClientSid;
}

wstring WindowStationPreparation::GetDesktopName()
{
	return L"ConsoleServer_" + SidToString(mClientSid);
}

wstring WindowStationPreparation::GetClientSidString()
{
	return SidToString(mClientSid);
}

wstring WindowStationPreparation::GetClientAndConsoleString()
{
	return mClientConsoleString;
}

void WindowStationPreparation::PreparationCompleted()
{
	LOG(INFO) << "WindowStationPreparation " << mClientConsoleString << " is completed";
	mIsPrepared = true;
}

bool WindowStationPreparation::IsPrepared()
{
	return mIsPrepared;
}

void WindowStationPreparation::OrderConsoleEvent(ConsoleEventOrder order)
{
	mConsoleEventOrders.push_back(order);
}

bool WindowStationPreparation::ReadConsoleEventOrder(ConsoleEventOrder & order)
{
	if (!mConsoleEventOrders.empty())
	{
		order = mConsoleEventOrders.front();
		mConsoleEventOrders.pop_front();
		return true;
	}
	else
		return false;
}

void WindowStationPreparation::StartPreparation()
{
	LOG(INFO) << "Starting preparation of window station for client with SID " << SidToString(mClientSid);

	// get console access token
	CAccessToken catConsoleToken;
	GetConsoleAccessToken(catConsoleToken);

	// execute PrepareWindowStation on console session
	wstring wsCmdLine = L"PrepareWindowStation.exe " + mPreparationId;

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.wShowWindow = SW_HIDE;
	startupInfo.dwFlags |= STARTF_USESHOWWINDOW;
	startupInfo.lpDesktop = L"WinSta0\\Default";

	PROCESS_INFORMATION processInformation;
	BOOL status = CreateProcessAsUser(
		catConsoleToken.GetHandle(),
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

	LOG(INFO) << "PrepareWindowStation.exe executed";
}


