#include "qemu/osdep.h"
#include "sysemu/block-backend.h"

__attribute__((weak))
int blk_commit_all(void)
{
    return 0;
}
