// Exec.h : Declaration of the CExec

#pragma once
#include "resource.h"       // main symbols

#include "ConsoleServer_i.h"

#include "PipeTools.h"

#include <memory>


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

using namespace ATL;


// CExec

class ATL_NO_VTABLE CExec :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CExec, &CLSID_Exec>,
	public IDispatchImpl<IExec, &IID_IExec, &LIBID_ConsoleServerLib, /*wMajor =*/ 1, /*wMinor =*/ 0>
{
public:

	DECLARE_REGISTRY_RESOURCEID(IDR_EXEC)
	BEGIN_COM_MAP(CExec)
		COM_INTERFACE_ENTRY(IExec)
		COM_INTERFACE_ENTRY(IDispatch)
	END_COM_MAP()
	DECLARE_PROTECT_FINAL_CONSTRUCT()

	CExec();
	virtual ~CExec();

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:

	STDMETHOD(StartProcess)(BSTR commandLine, BYTE *success, LONGLONG* error);
	STDMETHOD(ReadStdout)(BSTR* data, long *dataLength);
	STDMETHOD(ReadStderr)(BSTR* data, long *dataLength);
	STDMETHOD(WriteStdin)(BSTR data, long dataLength);
	STDMETHOD(GetTerminationStatus)(BYTE* hasTerminated, LONGLONG* exitCode);
	STDMETHOD(SendControl)(ControlEvent evt);
	STDMETHOD(Ping)();

protected:

	void TimerCallback();
	void KillProcess();

	void CreateProcessDesktop();
	void ReleaseProcessDesktop();

protected:
	const int MAX_PING_TIME = 10;

	bool processStarted;
	
	HANDLE mProcess;
	HANDLE hThread;
	DWORD dwProcessId;
	DWORD dwThreadId;

	HANDLE mStdoutRead;
	HANDLE mStdoutWrite;
	HANDLE mStderrRead;
	HANDLE mStderrWrite;
	HANDLE mStdinRead;
	HANDLE mStdinWrite;

	std::unique_ptr<PipeWriter> mStdinWriter;

	clock_t mLastPingTime;
	bool mProcessKilled;

	CHandle mPreparePipe;
	CHandle mPrepareReleaseEvent;
	std::wstring mProcessDesktopName;
};

OBJECT_ENTRY_AUTO(__uuidof(Exec), CExec)
