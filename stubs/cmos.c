#include "qemu/osdep.h"
#include "hw/block/fdc.h"

__attribute__((weak))
int cmos_get_fd_drive_type(FloppyDriveType fd0)
{
    return 0;
}
