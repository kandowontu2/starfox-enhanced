// PS5 process setup, run first in main(), following ProsperoEden's native
// startup (headless/main.cpp):
// - A native title is sandboxed: /app0 is read-only, /download0 is its own
//   writable storage, and /data is reachable only where the homebrew loader
//   grants it. The data folder is /data/StarFoxEnhanced when it is writable,
//   else the title folder when the loader mounts it writable (ShadowMountPlus
//   directory titles: /data/homebrew/<TITLE_ID>, reachable over FTP), else
//   /download0/StarFoxEnhanced.
// - RADV's shader cache defaults to /app0 (read-only); it is kept in the data
//   folder, and Mesa turns the disk cache off while the effective group
//   differs from the real one, so they are matched.
// - stderr (the game's diagnostics) goes to stderr.log there; the previous
//   run's log is kept as stderr.prev.log. SDL's log goes to sdl.log.
#include "native.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *data_path;

static bool PS_Writable(const char *directory)
{
    if (mkdir(directory, 0777) != 0 && errno != EEXIST) {
        return false;
    }
    char probe[256];
    snprintf(probe, sizeof(probe), "%s.write-test", directory);
    const int fd = open(probe, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return false;
    }
    const bool written = write(fd, "ok", 2) == 2;
    close(fd);
    unlink(probe);
    return written;
}

const char *StarfoxPS5_DataPath(void)
{
    if (!data_path) {
        if (PS_Writable(STARFOX_DATA_PATH)) {
            data_path = STARFOX_DATA_PATH;
        } else if (PS_Writable(STARFOX_TITLE_DATA_PATH)) {
            data_path = STARFOX_TITLE_DATA_PATH;
        } else {
            (void)mkdir(STARFOX_SANDBOX_DATA_PATH, 0777);
            data_path = STARFOX_SANDBOX_DATA_PATH;
        }
    }
    return data_path;
}

void StarfoxPS5_ProcessSetup(void)
{
    const char *data = StarfoxPS5_DataPath();
    char path[256];
    char previous[256];

    snprintf(path, sizeof(path), "%sstderr.log", data);
    snprintf(previous, sizeof(previous), "%sstderr.prev.log", data);
    (void)rename(path, previous);
    if (freopen(path, "w", stderr)) {
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    fprintf(stderr, "data folder: %s\n", data);

    if (getegid() != getgid() && setegid(getgid()) != 0) {
        fprintf(stderr, "could not match the effective group; RADV's shader cache stays off\n");
    }
    snprintf(path, sizeof(path), "%scache", data);
    (void)mkdir(path, 0777);
    snprintf(path, sizeof(path), "%scache/radv", data);
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        setenv("MESA_SHADER_CACHE_DIR", path, 1);
    }
}

void StarfoxPS5_HideSplashScreen(void)
{
    static bool hidden;
    if (hidden) {
        return;
    }
    const int result = sceSystemServiceHideSplashScreen();
    fprintf(stderr, "hide splash screen: 0x%08x\n", (unsigned)result);
    hidden = result == 0;
}

static FILE *sdl_log;

static void PS5_LogOutput(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    (void)userdata;
    fprintf(sdl_log, "%llu ms [%d:%d] %s\n", (unsigned long long)SDL_GetTicks(), category, (int)priority,
            message);
    fflush(sdl_log);
}

void StarfoxPS5_InstallLog(void)
{
    if (sdl_log) {
        return;
    }
    char path[256];
    snprintf(path, sizeof(path), "%ssdl.log", StarfoxPS5_DataPath());
    sdl_log = fopen(path, "a");
    if (!sdl_log) {
        return;
    }
    fprintf(sdl_log, "\nlaunch unix-seconds=%lld\n", (long long)time(NULL));
    fflush(sdl_log);
    SDL_SetLogOutputFunction(PS5_LogOutput, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
}
