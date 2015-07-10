// TestConsoleAttach.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>

using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
	DWORD pid = stoi(wstring(argv[1]));

	cout << "Attach to pid " << pid << endl;

	if (!FreeConsole())
		cerr << "FreeConsole: " << GetLastError() << endl;

	if (!AttachConsole(pid))
		cerr << "AttachConsole: " << GetLastError() << endl;

	if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0))
		cerr << "GenerateConsoleCtrlEvent: " << GetLastError() << endl;

	return 0;
}

