// PrepareWindowStationComm.h : Declaration of the CPrepareWindowStationComm

#pragma once
#include "resource.h"       // main symbols



#include "ConsoleServer_i.h"



#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

using namespace ATL;


// CPrepareWindowStationComm

class ATL_NO_VTABLE CPrepareWindowStationComm :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CPrepareWindowStationComm, &CLSID_PrepareWindowStationComm>,
	public IDispatchImpl<IPrepareWindowStationComm, &IID_IPrepareWindowStationComm, &LIBID_ConsoleServerLib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:
	DECLARE_REGISTRY_RESOURCEID(IDR_PREPAREWINDOWSTATIONCOMM)


	BEGIN_COM_MAP(CPrepareWindowStationComm)
		COM_INTERFACE_ENTRY(IPrepareWindowStationComm)
		COM_INTERFACE_ENTRY(IDispatch)
	END_COM_MAP()


	CPrepareWindowStationComm()
	{
	}


	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
	STDMETHOD(GetData)(BSTR preparationId, BSTR* desktopName, BSTR* clientSidString);
	STDMETHOD(IsActive)(BSTR preparationId, BYTE* active);
	STDMETHOD(PreparationCompleted)(BSTR preparationId);
};

OBJECT_ENTRY_AUTO(__uuidof(PrepareWindowStationComm), CPrepareWindowStationComm)
