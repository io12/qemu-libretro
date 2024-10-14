#include "qemu/osdep.h"
#include "sysemu/sysemu.h"

__attribute__((weak))
const char *qemu_get_vm_name(void)
{
    return NULL;
}

