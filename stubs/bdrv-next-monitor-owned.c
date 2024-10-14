#include "qemu/osdep.h"
#include "block/block.h"

__attribute__((weak))
BlockDriverState *bdrv_next_monitor_owned(BlockDriverState *bs)
{
    return NULL;
}
