/*
 * Win32 keyboard hook stubs
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#include "qemu/osdep.h"
#include "ui/win32-kbd-hook.h"

__attribute__((weak))
void win32_kbd_set_window(void *hwnd)
{
}

__attribute__((weak))
void win32_kbd_set_grab(bool grab)
{
}
