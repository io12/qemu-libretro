#include "qemu/osdep.h"
#include "sysemu/hw_accel.h"

__attribute__((weak))
void cpu_synchronize_state(CPUState *cpu)
{
}
__attribute__((weak))
void cpu_synchronize_post_init(CPUState *cpu)
{
}
