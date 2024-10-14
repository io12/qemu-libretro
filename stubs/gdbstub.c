#include "qemu/osdep.h"
#include "exec/gdbstub.h"       /* gdb_static_features */

__attribute__((weak))
const GDBFeature gdb_static_features[] = {
  { NULL }
};
