// Shared declarations for the PS5 runtime (SDL backend and process setup).
#pragma once
#include <SDL3/SDL.h>
#include "ps5_abi.h"

// Preferred data folder, the title folder when the loader mounts it writable,
// and the title's own sandbox storage (process.c chooses once per process).
#define STARFOX_DATA_PATH "/data/StarFoxEnhanced/"
#define STARFOX_TITLE_DATA_PATH "/app0/"
#define STARFOX_SANDBOX_DATA_PATH "/download0/StarFoxEnhanced/"
const char *StarfoxPS5_DataPath(void);
void StarfoxPS5_ProcessSetup(void);
void StarfoxPS5_InstallLog(void);
void StarfoxPS5_HideSplashScreen(void);
void StarfoxPS5_PollGamepads(void);
void StarfoxPS5_CloseGamepads(void);
