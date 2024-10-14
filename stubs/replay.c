#include "qemu/osdep.h"
#include "exec/replay-core.h"

__attribute__((weak))
void replay_finish(void)
{
}

__attribute__((weak))
void replay_save_random(int ret, void *buf, size_t len)
{
}

__attribute__((weak))
int replay_read_random(void *buf, size_t len)
{
    return 0;
}

__attribute__((weak))
bool replay_reverse_step(void)
{
    return false;
}

__attribute__((weak))
bool replay_reverse_continue(void)
{
    return false;
}
