#!/usr/bin/env bash
# Run the core's tests on one platform.
#
# usage: nix run .#test -- PLATFORM CORE
# platforms: linux-x64 linux-i686 win64 win32
#            android-x86_64 android-x86 android-arm64 android-arm
#            linux-x64-asan
#
# CORE is a built core, e.g. ci-build/win64/qemu_libretro.dll from
# `nix run .#ci -- win64`. Firmware comes from this checkout's pc-bios/.
# linux-x64-asan uses the sanitized frontend and runtime that its CI job
# leaves next to the core. Any ASan or UBSan report fails the test.
#
# Expects FRONTEND_<platform>, TEST_GUEST_DIR and the platform's tools (wine,
# the Android SDK, qemu-user and a bionic sysroot) from the flake's wrapper.
set -euo pipefail

usage() {
	sed -n '2,/^set /{/^#/s/^# \{0,1\}//p}' "$0"
	exit 2
}

[ $# = 2 ] || usage
# Crashing is a test failure, and qemu-user writes cores to the cwd
ulimit -c 0
platform=$1
core=$(realpath "$2")
pc_bios=$(git rev-parse --show-toplevel)/pc-bios
work=$(mktemp -d)
trap 'cleanup' EXIT
cleanup() {
	[ -n "${emulator_pid:-}" ] && kill "$emulator_pid" 2>/dev/null
	rm -rf "$work"
}

if [ "$platform" != linux-x64-asan ]; then
	frontend_var=FRONTEND_${platform//-/_}
	frontend=${!frontend_var:?no frontend for $platform}
fi

# Test cases: name, content, firmware?, frontend options, expected output
cases=(
	"boot|guest.img|yes|--until 640x400 --pixel 320,100=ff0000 --pixel 320,300=00ff00|"
	"no-firmware|guest.img|no|--frames 600|could not load PC BIOS"
	"zstd-qcow2|guest-zstd.qcow2|yes|--until 640x400 --pixel 320,100=ff0000 --pixel 320,300=00ff00|"
)

# Each platform defines setup (copy files where the frontend can reach them)
# and run DIR CONTENT SYSDIR OPTIONS... (run the frontend, printing its output)
case $platform in
linux-x64 | linux-i686)
	setup() { :; }
	run() { timeout 600 "$frontend" "$core" "$@"; }
	files=$work
	;;
linux-x64-asan)
	runtime=$(dirname "$core")/sanitizer-runtime
	frontend=$work/frontend
	# QEMU doesn't free everything at exit, so leak reports are noise
	export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1
	# Point the frontend at the image's loader and libraries (DT_RPATH,
	# unlike RUNPATH, also covers the core's dependencies). Running it through
	# the loader instead would make sanitizer reports name the loader for the
	# frontend's frames.
	setup() {
		cp "$(dirname "$core")/frontend" "$frontend"
		chmod u+w "$frontend"
		patchelf --set-interpreter "$runtime/ld-linux-x86-64.so.2" \
			--set-rpath "$runtime" --force-rpath "$frontend"
	}
	run() { timeout 600 "$frontend" "$core" "$@"; }
	files=$work
	;;
win64 | win32)
	export WINEPREFIX=$work/wineprefix WINEDEBUG=-all
	setup() { wineboot -i >/dev/null 2>&1; }
	run() {
		local content=$1 sys=$2
		shift 2
		timeout 600 wine "$frontend" "Z:$core" "Z:$content" "Z:$sys" "$@" \
			2>/dev/null
	}
	files=$work
	;;
android-x86_64 | android-x86)
	files=/data/local/tmp/qemu-libretro-test
	export ANDROID_USER_HOME=$work/android ANDROID_AVD_HOME=$work/android/avd
	setup() {
		echo "starting the Android emulator" >&2
		echo no | avdmanager create avd -n test -k "$ANDROID_SYSTEM_IMAGE" \
			>/dev/null
		emulator -avd test -no-window -no-audio -no-boot-anim \
			-no-snapshot -gpu swiftshader_indirect -memory 4096 \
			>"$work/emulator.log" 2>&1 &
		emulator_pid=$!
		adb wait-for-device
		until [ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = 1 ]; do
			sleep 2
		done
		adb shell "rm -rf $files && mkdir -p $files/sys $files/nosys"
		adb push "$frontend" "$files/frontend" >/dev/null
		adb push "$core" "$files/core.so" >/dev/null
		adb push "$pc_bios" "$files/sys/qemu" >/dev/null
		adb push "$TEST_GUEST_DIR"/. "$files/" >/dev/null
		# QEMU opens disk images read-write; the store's copies are read-only
		adb shell "chmod 755 $files/frontend && chmod 644 $files/guest*"
	}
	run() {
		timeout 600 adb shell "cd $files && ./frontend $files/core.so $*" |
			tr -d '\r'
	}
	;;
android-arm64 | android-arm)
	files=$work
	setup() { :; }
	run() {
		local qemu=qemu-aarch64 sysroot=$ANDROID_SYSROOT_ARM
		[ "$platform" = android-arm ] && qemu=qemu-arm
		# The frontend and core are bionic binaries; qemu-user runs them
		# with /system and /apex looked up in the system image's sysroot.
		# 32-bit bionic aborts on pids over 65535 (Android's pid_max is
		# 32768), so run in a new pid namespace.
		timeout 600 unshare --user --map-root-user --pid --fork \
			--kill-child "$qemu" -L "$sysroot" "$frontend" "$core" "$@"
	}
	;;
*)
	usage
	;;
esac

# Lay out the files for the frontend: content, and system dirs with and
# without firmware
case $platform in
android-x86_64 | android-x86) ;;
*)
	mkdir -p "$work/sys" "$work/nosys"
	ln -s "$pc_bios" "$work/sys/qemu"
	cp "$TEST_GUEST_DIR"/* "$work/"
	# QEMU opens disk images read-write; the store's copies are read-only
	chmod u+w "$work"/guest*
	;;
esac

setup
failed=0
for c in "${cases[@]}"; do
	IFS='|' read -r name content firmware opts expect <<<"$c"
	sys=$files/nosys
	[ "$firmware" = yes ] && sys=$files/sys
	# shellcheck disable=SC2086
	out=$(run "$files/$content" "$sys" $opts 2>&1) || true
	ok=1
	grep -q '^RESULT ok' <<<"$out" || ok=0
	[ -z "$expect" ] || grep -q "$expect" <<<"$out" || ok=0
	! grep -qE '^==[0-9]+==ERROR: |: runtime error: ' <<<"$out" || ok=0
	if [ $ok = 1 ]; then
		echo "PASS $platform $name"
	else
		echo "FAIL $platform $name"
		# Show the first sanitizer report if there is one, else the end
		report=$(awk '/^==[0-9]+==ERROR: |: runtime error: /{p=1} p' <<<"$out" |
			head -40)
		sed 's/^/    /' <<<"${report:-$(tail -20 <<<"$out")}"
		failed=1
	fi
done
exit $failed
