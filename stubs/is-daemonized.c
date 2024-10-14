#include "qemu/osdep.h"

/* Win32 has its own inline stub */
#ifndef _WIN32
__attribute__((weak))
bool is_daemonized(void)
{
    return false;
}
#endif
