# qemu-libretro

This is a libretro QEMU core, allowing to run VMs in a libretro frontend.

## Download

You can download this core from the [libretro Buildbot](https://buildbot.libretro.com/).

## Usage

You can open a `.iso`, `.img`, `.qcow`, or `.qcow2` file,
and it will be run with the command

``` sh
qemu-system-x86_64 -audiodev libretro,id=snd0 -machine pcspk-audiodev=snd0 -device AC97,audiodev=snd0 PATH_TO_OPENED_FILE
```

If you need more customization, such as increasing memory or using a different architecture than x86_64,
you can provide your own QEMU command in a `.qemu_cmd_line` file.
It uses the [`g_shell_parse_argv()`](https://docs.gtk.org/glib/func.shell_parse_argv.html) function and not an actual shell to parse the command, so it will only supports simple shell features.
The core interprets relative paths as relative to the directory containing the `.qemu_cmd_line` file.

## Examples

### KolibriOS

1. Download the [live CD image](https://builds.kolibrios.org/en_US/latest-iso.7z).
2. Open `kolibri.iso` with the QEMU core.

### Windows XP

1. Create an empty disk image with
   ``` sh
   qemu-img create -f qcow2 winxp_disk.qcow2 12G
   ```
   Change `12G` to the size of the disk you want.
   This core doesn't provide a way to run `qemu-img`, so you will need to make the disk image separately and copy it into the games folder.
2. Copy the installer ISO to `winxp_installer.iso` in the games folder.
3. Create a text file `winxp.qemu_cmd_line` in the games folder containing the command
   ``` sh
   qemu-system-x86_64 -hda winxp_disk.qcow2 -cdrom winxp_installer.iso -boot d -m 1G
   ```
4. Load `winxp.qemu_cmd_line` with the QEMU core and complete the installation.
5. Modify `winxp.qemu_cmd_line` to contain
   ``` sh
   qemu-system-x86_64 -hda winxp_disk.qcow2 -m 1G
   ```
   Change the `1G` to a different amount of RAM if desired. The `winxp_installer.iso` file isn't needed anymore and can be deleted.
6. Load `winxp.qemu_cmd_line` with the QEMU core to boot into Windows XP.

### Windows 11

1. Create an empty disk image with
   ``` sh
   qemu-img create -f qcow2 win11_disk.qcow2 64G
   ```
   Change `64G` to the size of the disk you want.
   This core doesn't provide a way to run `qemu-img`, so you will need to make the disk image separately and copy it into the games folder.
2. Copy the installer ISO to `win11_installer.iso` in the games folder.
3. Create a text file `win11.qemu_cmd_line` in the games folder containing the command
   ```sh
   qemu-system-x86_64 -hda win11_disk.qcow2 -cdrom win11_installer.iso -boot d -m 1G --cpu Skylake-Client-v3
   ```
4. Load `win11.qemu_cmd_line` with the QEMU core and complete the installation.
5. Modify `win11.qemu_cmd_line` to contain
   ```sh
   qemu-system-x86_64 -hda win11_disk.qcow2 -m 1G --cpu Skylake-Client-v3
   ```
   Change the `1G` to a different amount of RAM if desired. The `win11_installer.iso` file isn't needed anymore and can be deleted.
6. Load `win11.qemu_cmd_line` with the QEMU core to boot into Windows 11.

### MacOS 9.2

1. Create an empty disk image with
   ``` sh
   qemu-img create -f qcow2 mac9_disk.qcow2 2G
   ```
   Change `2G` to the size of the disk you want.
   This core doesn't provide a way to run `qemu-img`, so you will need to make the disk image separately and copy it into the games folder.
2. Copy the installer ISO to `mac9_installer.iso` in the games folder.
3. Create a text file `mac9.qemu_cmd_line` in the games folder containing the command
   ```sh
   qemu-system-ppc -boot d -m 512 -L pc-bios -hda mac9_disk.qcow2 -drive file=mac9_installer.iso,format=raw,media=cdrom -M mac99,via=pmu
   ```
4. Load `mac9.qemu_cmd_line` with the QEMU core and complete the installation.
   Open "Drive Setup" in the "Utilities" folder, and initialize the volume called "not initialized".
   Then open the "Mac OS Install" program and complete the installation.
5. Modify `mac9.qemu_cmd_line` to contain
   ``` sh
   qemu-system-ppc -boot c -m 512M -L pc-bios -hda mac9_disk.qcow2 -M mac99,via=pmu
   ```
   Change the `512M` to a different amount of RAM if desired. The `mac9_installer.iso` file isn't needed anymore and can be deleted.
6. Load `mac9.qemu_cmd_line` with the QEMU core to boot into MacOS 9.2.

## Compile instructions (Ubuntu)

```sh
sudo apt-get install build-essential python3 python3-venv ninja-build flex bison zlib1g-dev
git clone --recursive https://github.com/io12/qemu-libretro
cd qemu-libretro
mkdir build
cd build
CFLAGS="-Os -Wno-error -Wno-nested-externs -Wno-redundant-decls" ../configure \
    --without-default-features \
    --glib=internal \
    --zlib=internal \
    --disable-pie \
    --enable-fdt=internal \
    --disable-modules \
    --disable-plugins \
    --enable-libretro \
    --audio-drv-list=libretro \
    --disable-sdl \
    -Dwrap_mode=forcefallback
make -j$(nproc) libqemu_libretro.so
```

For help compiling for other platforms, see the [CI build script](libretro-gitlab-build.sh).

