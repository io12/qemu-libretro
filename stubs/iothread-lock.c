#include "qemu/osdep.h"
#include "qemu/main-loop.h"

__attribute__((weak))
bool bql_locked(void)
{
    return false;
}

__attribute__((weak))
void bql_lock_impl(const char *file, int line)
{
}

__attribute__((weak))
void bql_unlock(void)
{
}
