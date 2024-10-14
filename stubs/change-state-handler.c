#include "qemu/osdep.h"
#include "sysemu/runstate.h"

__attribute__((weak))
VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    return NULL;
}

__attribute__((weak))
void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    /* Nothing to do. */
}
