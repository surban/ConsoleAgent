#pragma once

#include "stdafx.h"
#include "ConsoleServer_i.h"

#include <map>
#include <list>
#include <deque>

using namespace std;
using namespace ATL;



struct ConsoleEventOrder
{
	DWORD ProcessId;
	ControlEvent Event;
};


class PreparationStillTerminating : public exception {};

class WindowStationPreparation
{
protected:
	WindowStationPreparation(wstring preparationId, wstring clientConsoleString, CSid clientSid);
public:
	virtual ~WindowStationPreparation();

	static shared_ptr<WindowStationPreparation> Prepare(CSid clientSid);
	static shared_ptr<WindowStationPreparation> GetPreparationById(wstring preparationId);

	static void PrepareProcessStarted(wstring clientConsoleString);
	static void PrepareProcessTerminated(wstring clientConsoleString);

	CSid GetClientSid();
	wstring GetDesktopName();
	wstring GetClientSidString();
	wstring GetClientAndConsoleString();

	void PreparationCompleted();
	bool IsPrepared();

	void OrderConsoleEvent(ConsoleEventOrder order);
	bool ReadConsoleEventOrder(ConsoleEventOrder &order);

protected:
	void StartPreparation();

	CSid mClientSid;
	wstring mPreparationId;
	wstring mClientConsoleString;
	bool mIsPrepared;
	deque<ConsoleEventOrder> mConsoleEventOrders;

	static map<wstring, weak_ptr<WindowStationPreparation>> sActivePrepationsById;
	static map<wstring, weak_ptr<WindowStationPreparation>> sActivePreparationsByClientAndConsole;
	static list<wstring> sRunningPrepareProcessesByClientAndConsole;

};
