#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ZIGPORTFOLIO_DIR="${ROOT_DIR}/zigPortfolio"
OUTPUT_DIR="${ROOT_DIR}/build_output"

RUN_TESTS=false
VERBOSE_TESTS=false

for arg in "$@"; do
  case "$arg" in
    --tests) RUN_TESTS=true ;;
    --verbose-tests) RUN_TESTS=true; VERBOSE_TESTS=true ;;
    *)
      echo "Unknown argument: $arg" >&2
      echo "Usage: $(basename "$0") [--tests] [--verbose-tests]" >&2
      exit 2
      ;;
  esac
done

echo "📦 Building zigPortfolio..."
echo "Root directory:       ${ROOT_DIR}"
echo "zigPortfolio folder:  ${ZIGPORTFOLIO_DIR}"
echo "Output directory:     ${OUTPUT_DIR}"
echo

mkdir -p "${OUTPUT_DIR}"

pushd "${ZIGPORTFOLIO_DIR}" >/dev/null
zig build

if $RUN_TESTS; then
  echo
  echo "🧪 Running unit tests..."
  if $VERBOSE_TESTS; then
    zig build test-verbose
  else
    zig build test
  fi
fi
popd >/dev/null

echo
echo "📁 Copying library and header to output folder..."

LIB_SRC="${ZIGPORTFOLIO_DIR}/zig-out/lib/libzigPortfolio.dylib"
HDR_SRC="${ZIGPORTFOLIO_DIR}/zig-out/zigPortfolio.h"

cp "${LIB_SRC}" "${OUTPUT_DIR}/libzigPortfolio.dylib"
cp "${HDR_SRC}" "${OUTPUT_DIR}/zigPortfolio.h"

echo "✅ Library copied to:"
echo "   ${OUTPUT_DIR}/libzigPortfolio.dylib"
echo "✅ Header copied to:"
echo "   ${OUTPUT_DIR}/zigPortfolio.h"
echo
echo "🎉 Done!"
