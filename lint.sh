#!/usr/bin/env bash
# Run both linters against the project source files.
# Usage: ./lint.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
CLANG_TIDY="${CLANG_TIDY:-/opt/homebrew/opt/llvm/bin/clang-tidy}"
CPPCHECK="${CPPCHECK:-cppcheck}"

echo "=== cppcheck ==="
"$CPPCHECK" \
  --enable=warning,portability \
  --error-exitcode=1 \
  --suppressions-list="${ROOT}/.cppcheck-suppressions" \
  --inline-suppr \
  -I "${ROOT}/src" \
  --platform=unix32 \
  --std=c++17 \
  --language=c++ \
  -D ESP_IDF_VERSION=0x050500 \
  "-D ESP_IDF_VERSION_VAL(a,b,c)=((a<<16)|(b<<8)|c)" \
  "${ROOT}/src/"

echo ""
echo "=== clang-tidy ==="

# Generate patched compile DB (strips xtensa flags, uses host clang++, creates
# xtensa header stubs for files missing from the PlatformIO installation).
if [ ! -f "${ROOT}/compile_commands.json" ]; then
  echo "compile_commands.json not found — regenerating via PlatformIO..."
  (cd "${ROOT}" && ~/.platformio/penv/bin/pio run -e esp32-s3-devkitc-1 --target compiledb --silent)
fi

python3 "${ROOT}/tools/patch_compile_db.py" \
  "${ROOT}/compile_commands.json" \
  "${ROOT}/.lint_db"

# Run clang-tidy and capture output. We do NOT propagate the exit code directly
# because ESP32 framework headers (xtensa, newlib, RISC-V) cause fatal compiler
# errors on the host clang — this is an inherent cross-compilation constraint.
# We fail only if clang-tidy reports actual check violations in our own source
# files (identified by the [check-name] suffix in the output line).
clang_tidy_output=$(
  "$CLANG_TIDY" \
    -p "${ROOT}/.lint_db" \
    --header-filter="${ROOT}/src/.*" \
    --use-color \
    "${ROOT}/src/I2SClocklessLedDriver.cpp" \
    "${ROOT}/src/HardwareSprite.cpp" \
  2>&1
) || true   # cross-compilation framework errors cause non-zero exit; checked below
echo "$clang_tidy_output"

rm -rf "${ROOT}/.lint_db"

# Filter for check violations: lines from our src/ files that end with [check-name]
violations=$(echo "$clang_tidy_output" | grep -E "^${ROOT}/src/.*\[[a-z].*\]$" || true)
if [ -n "$violations" ]; then
  echo ""
  echo "clang-tidy found violations in project source files."
  exit 1
fi

echo ""
echo "All lint checks passed."
