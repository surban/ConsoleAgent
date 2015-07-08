#pragma once

#include "stdafx.h"

#include <map>

using namespace std;
using namespace ATL;


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

	wstring GetDesktopName();
	wstring GetClientSidString();

	void PreparationCompleted();
	bool IsPrepared();


protected:
	CSid mClientSid;
	wstring mPreparationId;
	wstring mClientConsoleString;
	bool mIsPrepared;

	static map<wstring, weak_ptr<WindowStationPreparation>> sActivePrepationsById;
	static map<wstring, weak_ptr<WindowStationPreparation>> sActivePreparationsByClientAndConsole;

};