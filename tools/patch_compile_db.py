#!/usr/bin/env python3
"""
Patch compile_commands.json for clang-tidy on macOS.

PlatformIO generates a compile DB using the xtensa cross-compiler.
clang-tidy needs the native clang++ compiler and must not see xtensa-specific flags.
This script filters the DB to our project files, swaps the compiler, and
strips incompatible flags.

Usage:
    python3 tools/patch_compile_db.py [input_db] [output_dir]
    Defaults: compile_commands.json -> .lint_db/compile_commands.json
"""

import json
import os
import shlex
import sys

INPUT = sys.argv[1] if len(sys.argv) > 1 else "compile_commands.json"
OUTPUT_DIR = sys.argv[2] if len(sys.argv) > 2 else ".lint_db"

# Flags that clang does not understand (xtensa / gcc-only)
STRIP_FLAGS = {
    "-mlongcalls",
    "-fstrict-volatile-bitfields",
    "-fno-tree-switch-conversion",
    "-mdisable-hardware-atomics",
    "-fno-jump-tables",
    "-fuse-cxa-atexit",
    "-std=gnu++2b",
    "-std=gnu++2a",
    "-fno-rtti",
    "-freorder-blocks",
    "-fno-builtin-memcpy",
    "-fno-builtin-memset",
    "-fno-builtin-bzero",
    "-fno-builtin-stpcpy",
    "-fno-builtin-strncpy",
}

# Drop -I/-isystem paths that belong to the xtensa cross-toolchain; they contain
# headers that reference each other via relative paths (e.g. "../hal.h") which
# clang cannot resolve, causing fatal "file not found" errors that abort analysis.
STRIP_INCLUDE_SUBSTRINGS = {
    "toolchain-xtensa",
    "xtensa-esp32",
    # newlib/platform_include uses #include_next<sys/reent.h>, which requires a
    # downstream sys/reent.h that doesn't exist on macOS host clang.
    "newlib/platform_include",
}

# Override via CLANG env var; default to Homebrew path on macOS, plain clang++ on CI.
CLANG = os.environ.get("CLANG", "/opt/homebrew/opt/llvm/bin/clang++")

BASE = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

with open(INPUT) as f:
    db = json.load(f)

result = []
template = None

for entry in db:
    if "I2SClocklessLedDriver.cpp" not in entry["file"]:
        continue
    if ".platformio" in entry["file"]:
        continue

    parts = shlex.split(entry["command"])
    parts[0] = CLANG

    # Strip incompatible flags; normalise -std=; drop xtensa toolchain include paths.
    # PlatformIO emits combined -I<path> tokens (no space), so we only need that form.
    filtered, seen_std = [], False
    i = 0
    while i < len(parts):
        p = parts[i]
        if p in STRIP_FLAGS:
            pass
        elif p.startswith("-std="):
            if not seen_std:
                filtered.append("-std=c++17")
                seen_std = True
        elif p.startswith("-I"):
            path = p[2:]
            if not any(sub in path for sub in STRIP_INCLUDE_SUBSTRINGS):
                filtered.append(p)
        elif p == "-isystem" and i + 1 < len(parts):
            path = parts[i + 1]
            if not any(sub in path for sub in STRIP_INCLUDE_SUBSTRINGS):
                filtered.append(p)
                filtered.append(path)
            i += 1  # consume the path token regardless
        else:
            filtered.append(p)
        i += 1

    # Prepend a stub isystem path for newlib/platform headers missing on macOS.
    host_stubs = BASE + "/tools/host_stubs"
    filtered.insert(1, "-isystem" + host_stubs)

    entry = dict(entry)
    entry["command"] = " ".join(shlex.quote(p) for p in filtered)
    result.append(entry)
    template = entry
    break

# Add HardwareSprite.cpp using the same flags (fix up output and source paths)
if template:
    sprite = dict(template)
    sprite["file"] = os.path.join(BASE, "src", "HardwareSprite.cpp")
    sprite_parts = shlex.split(template["command"])
    for idx, part in enumerate(sprite_parts):
        if part.endswith("I2SClocklessLedDriver.cpp.o"):
            sprite_parts[idx] = part.replace(
                "I2SClocklessLedDriver.cpp.o", "HardwareSprite.cpp.o"
            )
        elif os.path.basename(part) == "I2SClocklessLedDriver.cpp":
            sprite_parts[idx] = os.path.join(BASE, "src", "HardwareSprite.cpp")
    sprite["command"] = " ".join(shlex.quote(p) for p in sprite_parts)
    result.append(sprite)

os.makedirs(OUTPUT_DIR, exist_ok=True)
out_path = os.path.join(OUTPUT_DIR, "compile_commands.json")
with open(out_path, "w") as f:
    json.dump(result, f, indent=2)

print(f"Wrote {len(result)} entries to {out_path}")

# Populate missing xtensa base headers in chip-specific include trees.
#
# Background: PlatformIO ships a chip-specific xtensa tree at e.g.
#   <fw-libs>/esp32s3/include/xtensa/esp32s3/include/xtensa/config/core.h
# core.h uses relative includes like `#include "../hal.h"`, expecting hal.h to
# sit one directory up in the same tree.  The actual base xtensa headers live in
# a sibling tree at <fw-libs>/esp32s3/include/xtensa/include/xtensa/.
# xtensa-gcc resolves this during toolchain build; clang-tidy running on the host
# doesn't have that sysroot, so those relative includes fail fatally.
#
# Fix: for every -I path whose xtensa/config/core.h exists but whose xtensa/ dir
# is missing xtensa base headers, copy the base headers from the sibling tree.
import shutil

if template:
    parts = shlex.split(template["command"])
    inc_paths = [p[2:] for p in parts if p.startswith("-I")]
    for inc_path in inc_paths:
        core_h = os.path.join(inc_path, "xtensa", "config", "core.h")
        if not os.path.exists(core_h):
            continue
        chip_xtensa_dir = os.path.join(inc_path, "xtensa")
        # Look for the sibling base xtensa dir: walk up to the xtensa/ root,
        # then look for include/xtensa/ next to the chip-specific tree.
        # Pattern: .../xtensa/<chip>/include  →  sibling: .../xtensa/include/xtensa/
        parent = os.path.dirname(os.path.dirname(inc_path))  # strip /<chip>/include
        base_xtensa = os.path.join(parent, "include", "xtensa")
        if not os.path.isdir(base_xtensa):
            continue
        for fname in os.listdir(base_xtensa):
            src = os.path.join(base_xtensa, fname)
            dst = os.path.join(chip_xtensa_dir, fname)
            if os.path.isfile(src) and not os.path.exists(dst):
                print(f"Copying xtensa stub {fname} → {dst}")
                shutil.copy2(src, dst)
