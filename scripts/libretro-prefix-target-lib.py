#!/usr/bin/env python3

import sys
import subprocess
import argparse
import tempfile
import functools
import operator


def make_substs(stdout, target, all_syms):
    syms = set()
    sections = set()
    for line in stdout.splitlines():
        tokens = line.split()
        if len(tokens) >= 2 and not tokens[0].startswith("."):
            if tokens[0].startswith("_") and not tokens[0].startswith("__emutls_v."):
                syms.add(f"{tokens[0]} _qemu_{target}_{tokens[0][1:]}\n")
            else:
                emutls = tokens[0].startswith("__emutls_v.")
                if emutls:
                    syms.add(
                        f"{tokens[0]} __emutls_v.qemu_{target}_{tokens[0][len('__emutls_v.'):]}\n"
                    )
                else:
                    syms.add(f"{tokens[0]} qemu_{target}_{tokens[0]}\n")
                if f".refptr.{tokens[0]}" in all_syms:
                    if emutls:
                        syms.add(
                            f".refptr.{tokens[0]} .refptr.__emutls_v.qemu_{target}_{tokens[0][len('__emutls_v.'):]}\n"
                        )
                        syms.add(
                            f".rdata$.refptr.{tokens[0]} .rdata$.refptr.__emutls_v.qemu_{target}_{tokens[0][len('__emutls_v.'):]}\n"
                        )
                        sections.add(
                            (
                                "--rename-section",
                                f".rdata$.refptr.{tokens[0]}=.rdata$.refptr.__emutls_v.qemu_{target}_{tokens[0][len('__emutls_v.'):]}",
                            )
                        )
                    else:
                        syms.add(
                            f".refptr.{tokens[0]} .refptr.qemu_{target}_{tokens[0]}\n"
                        )
                        syms.add(
                            f".rdata$.refptr.{tokens[0]} .rdata$.refptr.qemu_{target}_{tokens[0]}\n"
                        )
                        sections.add(
                            (
                                "--rename-section",
                                f".rdata$.refptr.{tokens[0]}=.rdata$.refptr.qemu_{target}_{tokens[0]}",
                            )
                        )
    return syms, sections


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host_os")
    ap.add_argument("nm")
    ap.add_argument("objcopy")
    ap.add_argument("target")
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args()

    pc = subprocess.run(
        [args.nm, "-P", args.input],
        text=True,
        stdout=subprocess.PIPE,
    )
    if pc.returncode != 0:
        sys.exit(1)

    all_syms = (
        set(line.split()[0] for line in pc.stdout.splitlines())
        if args.host_os == "windows"
        else set()
    )

    pc = subprocess.run(
        [args.nm, "-P", "--extern-only", "--defined-only", args.input],
        text=True,
        stdout=subprocess.PIPE,
    )
    if pc.returncode != 0:
        sys.exit(1)

    syms, sections = make_substs(pc.stdout, args.target, all_syms)
    syms = "".join(sorted(syms))
    sections = functools.reduce(operator.add, sections, tuple())

    renames_file = tempfile.NamedTemporaryFile(mode="w")
    renames_file.write(syms)
    renames_file.flush()

    pc = subprocess.run(
        [
            args.objcopy,
            f"--redefine-syms={renames_file.name}",
        ]
        + list(sections)
        + [
            args.input,
            args.output,
        ]
    )
    if pc.returncode != 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
