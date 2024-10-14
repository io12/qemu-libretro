#include "qemu/osdep.h"
#include "block/export.h"

/* Only used in programs that support block exports (libblockdev.a) */
__attribute__((weak))
void blk_exp_close_all(void)
{
}
