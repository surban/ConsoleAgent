#include "stdafx.h"
#include "PrepareWindowStationComm.h"

#include "WindowStationPreparation.h"

#include "utils.h"

// CPrepareWindowStationComm

CPrepareWindowStationComm::~CPrepareWindowStationComm()
{
	if (!mClientAndConsole.empty())
		WindowStationPreparation::PrepareProcessTerminated(mClientAndConsole);
}


STDMETHODIMP CPrepareWindowStationComm::Attach(BSTR preparationId)
{
	auto wsp = WindowStationPreparation::GetPreparationById(BStrToWString(preparationId));
	if (wsp == nullptr)
		return E_FAIL;
	
	mPreparation = wsp;
	mClientAndConsole = wsp->GetClientAndConsoleString();
	wsp->PrepareProcessStarted(mClientAndConsole);

	return S_OK;
}


STDMETHODIMP CPrepareWindowStationComm::GetData(BSTR* desktopName, BSTR* clientSidString)
{
	auto wsp = mPreparation.lock();
	if (wsp == nullptr)
		return E_FAIL;

	*desktopName = WStringToBStr(wsp->GetDesktopName());
	*clientSidString = WStringToBStr(wsp->GetClientSidString());
	return S_OK;
}


STDMETHODIMP CPrepareWindowStationComm::IsActive(BYTE* active)
{
	*active = !mPreparation.expired();
	return S_OK;
}


STDMETHODIMP CPrepareWindowStationComm::PreparationCompleted()
{
	auto wsp = mPreparation.lock();
	if (wsp == nullptr)
		return E_FAIL;

	wsp->PreparationCompleted();
	return S_OK;
}

STDMETHODIMP CPrepareWindowStationComm::GetConsoleEventOrder(BYTE *hasOrder, DWORD *processId, ControlEvent *evt)
{
	auto wsp = mPreparation.lock();
	if (wsp == nullptr)
		return E_FAIL;

	ConsoleEventOrder order;
	if (wsp->ReadConsoleEventOrder(order))
	{
		*hasOrder = true;
		*processId = order.ProcessId;
		*evt = order.Event;
	}
	else
		*hasOrder = false;

	return S_OK;
}


