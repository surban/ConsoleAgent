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
	if (!WTSQueryUserToken(dwConsoleSessionId, &hToken))
		throw runtime_error("Could not query console user token.");

	consoleToken.Attach(hToken);
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


