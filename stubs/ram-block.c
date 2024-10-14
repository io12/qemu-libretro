#include "qemu/osdep.h"
#include "exec/ramlist.h"
#include "exec/cpu-common.h"
#include "exec/memory.h"

__attribute__((weak))
void *qemu_ram_get_host_addr(RAMBlock *rb)
{
    return 0;
}

__attribute__((weak))
ram_addr_t qemu_ram_get_offset(RAMBlock *rb)
{
    return 0;
}

__attribute__((weak))
ram_addr_t qemu_ram_get_used_length(RAMBlock *rb)
{
    return 0;
}

__attribute__((weak))
void ram_block_notifier_add(RAMBlockNotifier *n)
{
}

__attribute__((weak))
void ram_block_notifier_remove(RAMBlockNotifier *n)
{
}

__attribute__((weak))
int qemu_ram_foreach_block(RAMBlockIterFunc func, void *opaque)
{
    return 0;
}

__attribute__((weak))
int ram_block_discard_disable(bool state)
{
    return 0;
}
