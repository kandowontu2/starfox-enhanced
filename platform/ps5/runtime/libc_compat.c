// FreeBSD libc functions SDL declares from the SDK headers but the console's
// libc module does not export. Each fails as an unsupported call, which is
// the behavior SDL already handles (SDL_gtk.c's own fallback for systems
// without them); GTK is never available on the console anyway.
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid)
{
    (void)ruid;
    (void)euid;
    (void)suid;
    errno = ENOSYS;
    return -1;
}

int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid)
{
    (void)rgid;
    (void)egid;
    (void)sgid;
    errno = ENOSYS;
    return -1;
}
