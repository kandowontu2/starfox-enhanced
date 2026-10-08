// Console paths are independent of the launcher working directory; the
// current directory still comes from SDL's POSIX filesystem operations.
#include "SDL_internal.h"
#include "filesystem/SDL_sysfilesystem.h"
#include "native.h"
#include <errno.h>
#include <sys/stat.h>

char *SDL_SYS_GetBasePath(void) { return SDL_strdup("/app0/"); }
char *SDL_SYS_GetExeName(void) { return SDL_strdup("StarFoxEnhanced"); }
char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    (void)org; (void)app;
    const char *data = StarfoxPS5_DataPath();
    if (mkdir(data, 0777) != 0 && errno != EEXIST) {
        SDL_SetError("Cannot create %s: %s", data, strerror(errno));
        return NULL;
    }
    return SDL_strdup(data);
}
char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    (void)folder;
    return SDL_strdup(StarfoxPS5_DataPath());
}
