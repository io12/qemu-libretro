#include "qemu/osdep.h"
#include "monitor/hmp-target.h"

__attribute__((weak))
const MonitorDef *target_monitor_defs(void)
{
    return NULL;
}
