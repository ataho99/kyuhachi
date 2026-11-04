// Android glue: DOSBox fopen_lock() returns NULL when flock() fails; shared storage (FUSE) may not support flock.
// Link-time wrap (--wrap=flock): log every failure, and treat "not supported" errno as success (no lock).
#include <sys/file.h>
#include <errno.h>
#include <android/log.h>
extern "C" int __real_flock(int fd, int op);
extern "C" int __wrap_flock(int fd, int op) {
    int r = __real_flock(fd, op);
    if (r < 0) {
        int e = errno;
        bool unsup = (e == ENOSYS || e == EOPNOTSUPP || e == EINVAL || e == ENOLCK || e == EPERM || e == EBADF);
        __android_log_print(ANDROID_LOG_WARN, "KHFLOCK", "flock(fd=%d, op=%d) failed errno=%d -> %s", fd, op, e, unsup ? "ignored (no lock)" : "kept as failure");
        if (unsup) { errno = 0; return 0; }
        errno = e;
    }
    return r;
}
