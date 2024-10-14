#include "qemu/osdep.h"
#include "qemu/main-loop.h"

__attribute__((weak))
bool qemu_in_main_thread(void)
{
    return qemu_get_current_aio_context() == qemu_get_aio_context();
}

