#include "qemu/osdep.h"
#include "migration/vmstate.h"

__attribute__((weak))
int vmstate_register_with_alias_id(VMStateIf *obj,
                                   uint32_t instance_id,
                                   const VMStateDescription *vmsd,
                                   void *base, int alias_id,
                                   int required_for_version,
                                   Error **errp)
{
    return 0;
}

__attribute__((weak))
void vmstate_unregister(VMStateIf *obj,
                        const VMStateDescription *vmsd,
                        void *opaque)
{
}

__attribute__((weak))
bool vmstate_check_only_migratable(const VMStateDescription *vmsd)
{
    return true;
}
