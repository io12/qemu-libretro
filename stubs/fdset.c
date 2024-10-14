#include "qemu/osdep.h"
#include "qapi/error.h"
#include "monitor/monitor.h"
#include "../monitor/monitor-internal.h"

__attribute__((weak))
int monitor_fdset_dup_fd_add(int64_t fdset_id, int flags, Error **errp)
{
    errno = ENOSYS;
    return -1;
}

__attribute__((weak))
void monitor_fdset_dup_fd_remove(int dupfd)
{
}

__attribute__((weak))
void monitor_fdsets_cleanup(void)
{
}
