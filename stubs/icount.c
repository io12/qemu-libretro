#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/cpu-timers.h"

/* icount - Instruction Counter API */

__attribute__((weak))
ICountMode use_icount = ICOUNT_DISABLED;

__attribute__((weak))
bool icount_configure(QemuOpts *opts, Error **errp)
{
    /* signal error */
    error_setg(errp, "cannot configure icount, TCG support not available");

    return false;
}
__attribute__((weak))
int64_t icount_get_raw(void)
{
    abort();
    return 0;
}
__attribute__((weak))
void icount_start_warp_timer(void)
{
    abort();
}
__attribute__((weak))
void icount_account_warp_timer(void)
{
    abort();
}
__attribute__((weak))
void icount_notify_exit(void)
{
    abort();
}
