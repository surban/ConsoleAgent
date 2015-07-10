
#include "stdafx.h"
#include "resource.h"
#include "ConsoleServer_i.h"
#include "Timers.h"
#include "utils.h"

using namespace ATL;


INITIALIZE_EASYLOGGINGPP;


class CConsoleServerModule : public ATL::CAtlServiceModuleT< CConsoleServerModule, IDS_SERVICENAME >
{
	typedef ATL::CAtlServiceModuleT< CConsoleServerModule, IDS_SERVICENAME > super;
public:
	DECLARE_LIBID(LIBID_ConsoleServerLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CONSOLESERVER, "{DB55AB7A-9650-4E6C-8085-73F44D2485C9}")

	HRESULT InitializeSecurity() throw()
	{
		// TODO : Call CoInitializeSecurity and provide the appropriate security settings for your service
		// Suggested - PKT Level Authentication, 
		// Impersonation Level of RPC_C_IMP_LEVEL_IDENTIFY 
		// and an appropriate Non NULL Security Descriptor.

		return S_OK;
	}

	HRESULT PreMessageLoop(int nShowCmd) throw()
	{
		// register timer
		StartThreadTimer(1000);

		return super::PreMessageLoop(nShowCmd);
	}
	
	HRESULT PostMessageLoop() throw ()
	{
		// deregister time
		StopThreadTimer();

		return super::PostMessageLoop();
	}

};


CConsoleServerModule _AtlModule;



extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, 
								LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	// configure logging
	ConfigureLogging();

	LOG(INFO) << "ConsoleServer startup";

	// main loop
	int retval = _AtlModule.WinMain(nShowCmd);

	LOG(INFO) << "ConsoleServer shutdown";
	return retval;
}

