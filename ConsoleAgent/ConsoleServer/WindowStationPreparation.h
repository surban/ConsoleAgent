#pragma once

#include "stdafx.h"

#include <map>
#include <list>

using namespace std;
using namespace ATL;


class PreparationStillTerminating : public exception {};


class WindowStationPreparation
{
protected:

	WindowStationPreparation(wstring preparationId, wstring clientConsoleString, CSid clientSid);

	void StartPreparation();


public:
	virtual ~WindowStationPreparation();

	CSid GetClientSid();

	static shared_ptr<WindowStationPreparation> Prepare(CSid clientSid);
	static shared_ptr<WindowStationPreparation> GetPreparationById(wstring preparationId);

	static void PrepareProcessStarted(wstring clientConsoleString);
	static void PrepareProcessTerminated(wstring clientConsoleString);

	wstring GetDesktopName();
	wstring GetClientSidString();
	wstring GetClientAndConsoleString();

	void PreparationCompleted();
	bool IsPrepared();


protected:
	CSid mClientSid;
	wstring mPreparationId;
	wstring mClientConsoleString;
	bool mIsPrepared;

	static map<wstring, weak_ptr<WindowStationPreparation>> sActivePrepationsById;
	static map<wstring, weak_ptr<WindowStationPreparation>> sActivePreparationsByClientAndConsole;
	static list<wstring> sRunningPrepareProcessesByClientAndConsole;

};