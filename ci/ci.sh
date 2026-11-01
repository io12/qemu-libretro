#!/usr/bin/env bash
# Run libretro's GitLab CI jobs for this core locally, in the same build images.
#
# usage: nix run .#ci -- JOB...
# jobs:  win64 win32 linux-x64 linux-i686
#        android-arm64 android-arm android-x86_64 android-x86
#        linux-x64-asan (not a libretro job: linux-x64 with ASan and UBSan,
#        unstripped, plus a sanitized test frontend and its runtime)
#
# Builds the image for each job from libretro's Dockerfile (pinned in
# flake.nix) with buildah, runs the job the way ci-templates does, and copies
# the stripped core to ci-build/JOB/. Images and job trees are kept under
# $QEMU_LIBRETRO_CI_CACHE (default ~/.cache/qemu-libretro-ci).
#
# Expects buildah, util-linux and coreutils in PATH, and CI_DOCKERFILE_<image>
# and CI_POLICY set, as the flake's wrapper does.
set -euo pipefail

usage() {
	sed -n '2,/^set /{/^#/s/^# \{0,1\}//p}' "$0"
	exit 2
}

[ $# -gt 0 ] || usage
case $1 in -h | --help) usage ;; esac

# buildah needs a user namespace with a range of ids (apt and MXE's tarballs
# use several). Enter one before doing anything else.
if [ -z "${QEMU_LIBRETRO_CI_USERNS:-}" ]; then
	export QEMU_LIBRETRO_CI_USERNS=1
	if [ "$(id -u)" = 0 ]; then
		# Map as many ids as this namespace has, up to 65536
		n=$(awk '$1 == 0 { print $3 }' /proc/self/uid_map)
		[ "$n" -gt 65536 ] && n=65536
		exec unshare --user --map-users="0:0:$n" --map-groups="0:0:$n" \
			--mount "$BASH" "$0" "$@"
	else
		exec buildah unshare "$BASH" "$0" "$@"
	fi
fi

src=$(git rev-parse --show-toplevel)
cache=${QEMU_LIBRETRO_CI_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/qemu-libretro-ci}
mkdir -p "$cache/tmp"
# buildah overlay-mounts build contexts under TMPDIR, which fails when that's
# on overlayfs (e.g. in a container); a tmpfs in our mount namespace works
# everywhere
mount -t tmpfs -o size=50% tmpfs "$cache/tmp"
export TMPDIR=$cache/tmp
B=(buildah --root "$cache/storage" --runroot "$cache/run"
   --storage-driver "${QEMU_LIBRETRO_CI_STORAGE_DRIVER:-vfs}")
numproc=$(nproc)

for sub in zlib libretro-common; do
	if [ ! -e "$src/$sub/.git" ]; then
		echo "$sub isn't checked out; run: git submodule update --init $sub" >&2
		exit 1
	fi
done

# Prints the tag of the image, building it if needed
image() {
	local name=$1 var=CI_DOCKERFILE_$1 tag ctx
	ctx=$(mktemp -d)
	# Changes for running in a user namespace rather than on GitLab's
	# runners: apt can't drop to its sandbox user, and tar can't restore
	# owners outside the namespace's id range.
	awk '{ print } /^FROM / && !done {
		print "RUN mkdir -p /etc/apt/apt.conf.d && echo '\''APT::Sandbox::User \"root\";'\'' > /etc/apt/apt.conf.d/99-sandbox-root"
		print "RUN printf '\''#!/bin/sh\\nexec %s \"$@\" --no-same-owner\\n'\'' \"$(command -v tar)\" > /usr/local/bin/tar && chmod +x /usr/local/bin/tar"
		done = 1 }' "${!var}" >"$ctx/Dockerfile"
	tag=localhost/qemu-libretro-ci-$name:$(sha256sum "$ctx/Dockerfile" | cut -c1-12)
	if ! "${B[@]}" inspect --type image "$tag" >/dev/null 2>&1; then
		echo "building image $tag (this takes a while the first time)" >&2
		if ! "${B[@]}" build --signature-policy "$CI_POLICY" \
			--isolation chroot --layers --build-arg NUMPROC="$numproc" \
			-t "$tag" "$ctx" >&2; then
			rm -rf "$ctx"
			return 1
		fi
	fi
	rm -rf "$ctx"
	echo "$tag"
}

# Variables from libretro-infrastructure/ci-templates
job_vars() {
	case $1 in
	win64) echo mxe qemu_libretro.dll strip platform=win64 ARCH=x86_64 \
		MSYSTEM=MINGW64 AR=x86_64-w64-mingw32.static-ar \
		AS=x86_64-w64-mingw32.static-as CC=x86_64-w64-mingw32.static-gcc \
		CXX=x86_64-w64-mingw32.static-g++ \
		WINDRES=x86_64-w64-mingw32.static-windres ;;
	win32) echo mxe qemu_libretro.dll strip platform=win32 ARCH=x86 \
		MSYSTEM=MINGW32 AR=i686-w64-mingw32.static-ar \
		AS=i686-w64-mingw32.static-as CC=i686-w64-mingw32.static-gcc \
		CXX=i686-w64-mingw32.static-g++ \
		WINDRES=i686-w64-mingw32.static-windres ;;
	linux-x64) echo amd64 qemu_libretro.so strip platform=unix CC=gcc \
		CXX=g++ ;;
	linux-i686) echo i386 qemu_libretro.so strip platform=unix ARCH=x86 \
		CC=gcc CXX=g++ ;;
	linux-x64-asan) echo amd64 qemu_libretro.so : platform=unix CC=gcc \
		CXX=g++ QEMU_LIBRETRO_CONFIGURE_ARGS=--enable-sanitizers \
		SANITIZER_FRONTEND=1 ;;
	android-arm64 | android-arm | android-x86_64 | android-x86)
		echo android qemu_libretro_android.so llvm-strip \
			ANDROID_PLATFORM="$1" ;;
	*) return 1 ;;
	esac
}

run_job() {
	local job=$1 vars img artifact strip tag c entrypoint work out
	vars=$(job_vars "$job") || { echo "unknown job $job" >&2; return 1; }
	read -r img artifact strip vars <<<"$vars"
	tag=$(image "$img") || { echo "building the $img image failed" >&2; return 1; }

	work=$cache/jobs/$job
	rm -rf "$work"
	mkdir -p "$work/src"
	tar -C "$src" --exclude=./.git --exclude=./build --exclude=./ci-build \
		--exclude='./result*' -cf - . | tar -C "$work/src" -xf -

	local env=(--env CORENAME=qemu --env MAKEFILE=Makefile.libretro-gitlab
		   --env MAKEFILE_PATH=. --env STRIP_CORE_LIB=1)
	for kv in $vars; do
		env+=(--env "$kv")
	done

	c=$("${B[@]}" from -q --pull=never "$tag")
	# GitLab runs the job through the image's entrypoint (linux32 on i386)
	entrypoint=$("${B[@]}" inspect --type image \
		--format '{{range .OCIv1.Config.Entrypoint}}{{.}} {{end}}' "$tag")
	echo "running $job in $tag" >&2
	local rc=0
	"${B[@]}" run --isolation chroot "${env[@]}" --env NUMPROC="$numproc" \
		-v "$work/src:/builds/qemu" --workingdir /builds/qemu "$c" -- \
		$entrypoint bash -c '
			set -e
			if [ -n "${NDK_ROOT:-}" ]; then
				export ANDROID_NDK_ROOT=$NDK_ROOT
				export ANDROID_NDK_LLVM=$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64
				export ANDROID_NDK_CLANG=$ANDROID_NDK_LLVM
				export PATH=$ANDROID_NDK_LLVM/bin:$PATH
			fi
			make -C "$MAKEFILE_PATH" -f "$MAKEFILE" -j"$NUMPROC" \
				${ANDROID_PLATFORM:+platform=$ANDROID_PLATFORM}
			'"$strip"' --strip-unneeded '"$artifact"'
			if [ -n "${SANITIZER_FRONTEND:-}" ]; then
				bash tests/libretro/build-sanitizer-frontend.sh '"$artifact"'
			fi
		' >"$work/log" 2>&1 || rc=$?
	"${B[@]}" rm "$c" >/dev/null

	out=$src/ci-build/$job
	mkdir -p "$out"
	cp "$work/log" "$out/log"
	if [ $rc != 0 ]; then
		echo "$job failed (exit $rc); log: $out/log" >&2
		tail -20 "$out/log" >&2
		return 1
	fi
	cp "$work/src/$artifact" "$out/"
	if [ -e "$work/src/sanitizer-runtime" ]; then
		rm -rf "$out/sanitizer-runtime"
		cp -r "$work/src/frontend" "$work/src/sanitizer-runtime" "$out/"
	fi
	echo "$job: $out/$artifact" >&2
}

failed=()
for job in "$@"; do
	run_job "$job" || failed+=("$job")
done
if [ ${#failed[@]} -gt 0 ]; then
	echo "failed: ${failed[*]}" >&2
	exit 1
fi
