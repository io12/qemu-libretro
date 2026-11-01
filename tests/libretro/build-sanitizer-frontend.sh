#!/usr/bin/env bash
# Build the test frontend with ASan and UBSan, and gather the loader and
# libraries it and the core need into sanitizer-runtime/.
#
# Runs in the build image after a --enable-sanitizers build of the core (the
# linux-x64-asan job in ci/ci.sh), so the frontend, the core and the
# sanitizer runtime all come from the image's compiler. The tests run the
# frontend through the gathered loader, so the host's libc doesn't matter.
set -euo pipefail

core=$1
gcc -O1 -g -Wall -fsanitize=address,undefined -Ilibretro-common/include \
	tests/libretro/frontend.c -o frontend -ldl

rm -rf sanitizer-runtime
mkdir sanitizer-runtime
ldd frontend "$core" |
	awk '$2 == "=>" && $3 ~ /^\// { print $3 } $1 ~ /^\// { print $1 }' |
	sort -u |
	while read -r lib; do
		cp -L "$lib" sanitizer-runtime/
	done
test -e sanitizer-runtime/ld-linux-x86-64.so.2
