#include "qemu/osdep.h"
#include "sysemu/cpu-timers.h"
#include "qemu/main-loop.h"

__attribute__((weak))
int64_t cpu_get_clock(void)
{
    return get_clock_realtime();
}
