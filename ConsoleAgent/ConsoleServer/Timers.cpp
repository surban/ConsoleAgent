#include "stdafx.h"

#include <map>

#include "Timers.h"


static std::map<void *, TimerCallback> RegisteredTimers;
static UINT_PTR TimerId;

void RegisterTimer(void *ref, TimerCallback callback)
{
	RegisteredTimers[ref] = callback;
}

void DeregisterTimer(void *ref)
{
	RegisteredTimers.erase(ref);
}

VOID CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	for (auto &item : RegisteredTimers)
		item.second();
}

void StartThreadTimer(unsigned int interval)
{
	LOG(INFO) << "Starting timer with interval " << interval << " ms";
	TimerId = SetTimer(NULL, 0, interval, &TimerProc);
}

void StopThreadTimer()
{
	LOG(INFO) << "Stopping timer";
	KillTimer(NULL, TimerId);
}