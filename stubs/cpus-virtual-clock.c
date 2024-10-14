#include "qemu/osdep.h"
#include "sysemu/cpu-timers.h"
#include "qemu/main-loop.h"

__attribute__((weak))
int64_t cpus_get_virtual_clock(void)
{
    return cpu_get_clock();
}

__attribute__((weak))
void cpus_set_virtual_clock(int64_t new_time)
{
    /* do nothing */
}
