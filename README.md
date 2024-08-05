# qemu-libretro

This is a work-in-progress libretro QEMU core,
but it seems to mostly function so far.

## Compile instructions (Linux)

First install the build dependencies.
I haven't determined what the full list is yet,
but at least `ninja`, `glib`, `pkg-config`, `flex`, and `bison`.

Then add `pic: true` to the `libfdt = library()` definition in `subprojects/dtc/libfdt/meson.build`.

Then run

```
mkdir build
cd build
CFLAGS=-Wno-error ../configure --without-default-features --target-list=i386-softmmu --glib=internal --zlib=internal --disable-pie --enable-libretro
make
```

Then `libqemu-system-i386.so` will be the core file.
You can run it with `retroarch -L libqemu-system-i386.so disk.iso` with an ISO file.
Note that you need to copy `bios-256k.bin`, `efi-e1000.rom`, `kvmvapic.bin`, and `vgabios-stdvga.bin` to `system/qemu` in your RetroArch directory.
I tested with [KolibriOS](https://builds.kolibrios.org/en_US/latest-iso.7z).
