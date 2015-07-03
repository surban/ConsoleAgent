// ConsoleServer.cpp : Implementation of WinMain


#include "stdafx.h"
#include "resource.h"
#include "ConsoleServer_i.h"
#include "Timers.h"

using namespace ATL;


class CConsoleServerModule : public ATL::CAtlExeModuleT< CConsoleServerModule >
{
public :
	DECLARE_LIBID(LIBID_ConsoleServerLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CONSOLESERVER, "{962952B2-E216-40E8-9155-21578AB4B1D8}")
	};

CConsoleServerModule _AtlModule;


INITIALIZE_EASYLOGGINGPP;


extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, 
								LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	// configure logging
	el::Configurations logConf;
	logConf.setToDefault();
	logConf.set(el::Level::Global, el::ConfigurationType::Filename, "c:\\temp\\ConsoleServer.log");
	el::Loggers::reconfigureAllLoggers(logConf);

	LOG(INFO) << "ConsoleServer startup";

	// allocate console
	//BOOL success = AllocConsole();
	//if (!success)
	//{
	//	LOG(ERROR) << "Failed to allocate console";
	//	abort();
	//}
	//ShowWindow(GetConsoleWindow(), SW_HIDE);

	// register timer
	StartThreadTimer(1000);

	// main loop
	int retval = _AtlModule.WinMain(nShowCmd);

	LOG(INFO) << "ConsoleServer shutdown";
	return retval;
}

