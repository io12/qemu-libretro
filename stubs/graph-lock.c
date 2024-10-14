#include "qemu/osdep.h"
#include "block/graph-lock.h"

__attribute__((weak))
void register_aiocontext(AioContext *ctx)
{
}

__attribute__((weak))
void unregister_aiocontext(AioContext *ctx)
{
}
