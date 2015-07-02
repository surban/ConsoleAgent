
#include "stdafx.h"

using namespace std;


ConsoleServerLib::IExecPtr pExec;




int _tmain(int argc, _TCHAR* argv[])
{
	CoInitialize(NULL);

	// usage
	if (argc < 2)
	{
		wcerr << "Usage: " << argv[0] << " <program> <arguments>" << endl;
		wcerr << endl;
		wcerr << "Executes the specified program on the console session." << endl;
		return -2;
	}
	
	try 
	{
		// connect to COM server
		pExec.CreateInstance(__uuidof(ConsoleServerLib::Exec));

		// build command line
		wstring cmdLine;
		for (int n = 1; n < argc; n++) 
		{
			if (n > 1)
				cmdLine += L" ";
			cmdLine += argv[n];
		}
		_bstr_t bsCmdLine(cmdLine.c_str());

		// start process on console session
		BYTE success;
		LONGLONG error;
		pExec->StartProcess(bsCmdLine.GetBSTR(), &success, &error);

		// process start failure
		if (!success)
		{
			_com_error err((HRESULT)error);
			wcerr << "Process launch failure: " << err.ErrorMessage() << endl;
			return -1;
		}

		cout << "Process started" << std::endl;

		// read stdout
		
	}
	catch (_com_error err) {
		wcerr << "==========================================================" << endl;
		wcerr << "COM error: 0x" << hex << err.Error() << " - " << err.ErrorMessage() << endl;
		wcerr << "Check that ConsoleServer is properly installed." << endl;
		return -3;
	}

	return 0;
}

