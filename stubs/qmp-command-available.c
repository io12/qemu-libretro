#include "qemu/osdep.h"
#include "qapi/qmp/dispatch.h"

__attribute__((weak))
bool qmp_command_available(const QmpCommand *cmd, Error **errp)
{
    return true;
}
