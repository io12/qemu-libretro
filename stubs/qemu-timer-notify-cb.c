#include "qemu/osdep.h"
#include "sysemu/cpu-timers.h"
#include "qemu/main-loop.h"

__attribute__((weak))
void qemu_timer_notify_cb(void *opaque, QEMUClockType type)
{
    qemu_notify_event();
}
