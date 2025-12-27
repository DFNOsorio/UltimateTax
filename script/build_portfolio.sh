#!/bin/bash

set -e  # exit on error

usage() {
    echo "Usage: $(basename "$0") [--tests|-t] [--verbose-tests|-v] [--trace N]"
    echo ""
    echo "Options:"
    echo "  -t, --tests          Build and run Zig unit tests"
    echo "  -v, --verbose-tests  Run unit tests with verbose output (requires build.zig step: test-verbose)"
    echo "      --trace N        Add -freference-trace=N to the test build (useful on failures)"
    echo "  -h, --help           Show this help"
}

RUN_TESTS=0
VERBOSE_TESTS=0
TRACE_N=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -t|--tests)
            RUN_TESTS=1
            shift
            ;;
        -v|--verbose-tests)
            VERBOSE_TESTS=1
            shift
            ;;
        --trace)
            TRACE_N="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $1"
            echo ""
            usage
            exit 2
            ;;
    esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORTFOLIO_DIR="$ROOT_DIR/zigPortfolio"
OUTPUT_DIR="$ROOT_DIR/build_output"

echo "📦 Building zigPortfolio..."
echo "Root directory:       $ROOT_DIR"
echo "zigPortfolio folder:  $PORTFOLIO_DIR"
echo "Output directory:     $OUTPUT_DIR"
echo ""

mkdir -p "$OUTPUT_DIR"

cd "$PORTFOLIO_DIR"
zig build

if [[ $RUN_TESTS -eq 1 ]]; then
    echo ""
    echo "🧪 Running unit tests..."

    # Compile-time args for zig (e.g., reference trace)
    ZIG_TEST_BUILD_ARGS=()
    if [[ -n "$TRACE_N" ]]; then
        ZIG_TEST_BUILD_ARGS+=("-freference-trace=$TRACE_N")
    fi

    if [[ $VERBOSE_TESTS -eq 1 ]]; then
        # Requires you to add a "test-verbose" step in build.zig that runs stdio-inherited tests.
        zig build test-verbose "${ZIG_TEST_BUILD_ARGS[@]}"
    else
        zig build test "${ZIG_TEST_BUILD_ARGS[@]}"
    fi
fi

echo ""
echo "📁 Copying library and header to output folder..."

LIB_PATH="$PORTFOLIO_DIR/zig-out/lib/libzigPortfolio.dylib"
HEADER_PATH="$PORTFOLIO_DIR/zig-out/zigPortfolio.h"

if [ -f "$LIB_PATH" ]; then
    cp "$LIB_PATH" "$OUTPUT_DIR/"
    echo "✅ Library copied to:"
    echo "   $OUTPUT_DIR/libzigPortfolio.dylib"
else
    echo "❌ ERROR: Could not find compiled library:"
    echo "   $LIB_PATH"
    exit 1
fi

if [ -f "$HEADER_PATH" ]; then
    cp "$HEADER_PATH" "$OUTPUT_DIR/"
    echo "✅ Header copied to:"
    echo "   $OUTPUT_DIR/zigPortfolio.h"
else
    echo "⚠️  WARNING: Header not found at:"
    echo "   $HEADER_PATH"
fi

# Clean zig-out if you still want to
rm -rf "$PORTFOLIO_DIR/zig-out"

echo ""
echo "🎉 Done!"
