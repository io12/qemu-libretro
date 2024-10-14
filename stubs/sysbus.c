#include "qemu/osdep.h"
#include "hw/qdev-core.h"

__attribute__((weak))
BusState *sysbus_get_default(void)
{
    return NULL;
}
