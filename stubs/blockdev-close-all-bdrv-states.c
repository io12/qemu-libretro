#include "qemu/osdep.h"
#include "block/block_int.h"

__attribute__((weak))
void blockdev_close_all_bdrv_states(void)
{
}
