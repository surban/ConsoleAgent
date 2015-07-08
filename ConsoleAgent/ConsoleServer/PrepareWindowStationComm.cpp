#include "stdafx.h"
#include "PrepareWindowStationComm.h"

#include "WindowStationPreparation.h"

#include "utils.h"

// CPrepareWindowStationComm


STDMETHODIMP CPrepareWindowStationComm::GetData(BSTR preparationId, BSTR* desktopName, BSTR* clientSidString)
{
	auto wsp = WindowStationPreparation::GetPreparationById(BStrToWString(preparationId));
	if (wsp == nullptr)
	{
		*desktopName = NULL;
		*clientSidString = NULL;
		return E_FAIL;
	}

	*desktopName = WStringToBStr(wsp->GetDesktopName());
	*clientSidString = WStringToBStr(wsp->GetClientSidString());
	return S_OK;
}


STDMETHODIMP CPrepareWindowStationComm::IsActive(BSTR preparationId, BYTE* active)
{
	auto wsp = WindowStationPreparation::GetPreparationById(BStrToWString(preparationId));
	
	if (wsp == nullptr)
		*active = FALSE;
	else
		*active = TRUE;

	return S_OK;
}


STDMETHODIMP CPrepareWindowStationComm::PreparationCompleted(BSTR preparationId)
{
	auto wsp = WindowStationPreparation::GetPreparationById(BStrToWString(preparationId));
	if (wsp == nullptr)
		return E_FAIL;

	wsp->PreparationCompleted();
	return S_OK;
}
