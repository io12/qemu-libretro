#!/usr/bin/env python3

import sys
import subprocess
import argparse
import tempfile


def make_substs(stdout, target):
    ret = set()
    for line in stdout.splitlines():
        tokens = line.split()
        if len(tokens) >= 2 and not tokens[0].startswith("."):
            if tokens[0].startswith("_"):
                ret.add(f"{tokens[0]} _qemu_{target}_{tokens[0][1:]}\n")
            else:
                ret.add(f"{tokens[0]} qemu_{target}_{tokens[0]}\n")
    return ret


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("nm")
    ap.add_argument("objcopy")
    ap.add_argument("target")
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args()

    pc = subprocess.run(
        [args.nm, "-P", "--extern-only", "--defined-only", args.input],
        text=True,
        stdout=subprocess.PIPE,
    )
    if pc.returncode != 0:
        sys.exit(1)
    syms = make_substs(pc.stdout, args.target)
    syms = "".join(sorted(syms))
    syms_file = tempfile.NamedTemporaryFile(mode="w")
    syms_file.write(syms)
    syms_file.flush()

    pc = subprocess.run(
        [args.objcopy, f"--redefine-syms={syms_file.name}", args.input, args.output]
    )
    if pc.returncode != 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
