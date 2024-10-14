#include "qemu/osdep.h"
#include "exec/cpu-common.h"

__attribute__((weak))
RAMBlock *qemu_ram_block_from_host(void *ptr, bool round_offset,
                                   ram_addr_t *offset)
{
    return NULL;
}

__attribute__((weak))
int qemu_ram_get_fd(RAMBlock *rb)
{
    return -1;
}
