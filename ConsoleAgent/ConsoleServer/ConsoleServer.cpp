// ConsoleServer.cpp : Implementation of WinMain


#include "stdafx.h"
#include "resource.h"
#include "ConsoleServer_i.h"
#include "Timers.h"
#include "utils.h"

using namespace ATL;

/*
class CConsoleServerModule : public ATL::CAtlExeModuleT< CConsoleServerModule >
{
public :
	DECLARE_LIBID(LIBID_ConsoleServerLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CONSOLESERVER, "{962952B2-E216-40E8-9155-21578AB4B1D8}")
	};
*/

class CConsoleServerModule : public ATL::CAtlServiceModuleT< CConsoleServerModule, IDS_SERVICENAME >
{
public:
	DECLARE_LIBID(LIBID_ConsoleServerLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CONSOLESERVER, "{962952B2-E216-40E8-9155-21578AB4B1D8}")

	HRESULT InitializeSecurity() throw()
	{
		// TODO : Call CoInitializeSecurity and provide the appropriate security settings for your service
		// Suggested - PKT Level Authentication, 
		// Impersonation Level of RPC_C_IMP_LEVEL_IDENTIFY 
		// and an appropriate Non NULL Security Descriptor.

		return S_OK;
	}
};


CConsoleServerModule _AtlModule;


INITIALIZE_EASYLOGGINGPP;


extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, 
								LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	// configure logging
	ConfigureLogging();

	LOG(INFO) << "ConsoleServer startup";

	// register timer
	StartThreadTimer(1000);

	// main loop
	int retval = _AtlModule.WinMain(nShowCmd);

	LOG(INFO) << "ConsoleServer shutdown";
	return retval;
}

