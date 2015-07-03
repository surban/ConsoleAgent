#pragma once

#include <functional>

typedef std::function<void()> TimerCallback;

void RegisterTimer(void *ref, TimerCallback callback);
void DeregisterTimer(void *ref);
void StartThreadTimer(unsigned int interval);

