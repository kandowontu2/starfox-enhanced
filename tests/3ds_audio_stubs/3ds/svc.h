#pragma once
#include <3ds/types.h>
u64 svcGetSystemTick(void);
Result svcSleepThread(int64_t nanoseconds);
