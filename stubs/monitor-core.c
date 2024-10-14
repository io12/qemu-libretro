#include "qemu/osdep.h"
#include "monitor/monitor.h"
#include "qapi/qapi-emit-events.h"

__attribute__((weak))
Monitor *monitor_cur(void)
{
    return NULL;
}

__attribute__((weak))
Monitor *monitor_set_cur(Coroutine *co, Monitor *mon)
{
    return NULL;
}

__attribute__((weak))
void qapi_event_emit(QAPIEvent event, QDict *qdict)
{
}

__attribute__((weak))
int monitor_vprintf(Monitor *mon, const char *fmt, va_list ap)
{
    abort();
}
