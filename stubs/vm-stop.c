#include "qemu/osdep.h"

#include "sysemu/runstate.h"
__attribute__((weak))
void qemu_system_vmstop_request_prepare(void)
{
    abort();
}

__attribute__((weak))
void qemu_system_vmstop_request(RunState state)
{
    abort();
}
