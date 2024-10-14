#include "qemu/osdep.h"

#include "sysemu/runstate.h"
__attribute__((weak))
bool runstate_check(RunState state)
{
    return state == RUN_STATE_PRELAUNCH;
}
