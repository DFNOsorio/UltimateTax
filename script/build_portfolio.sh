#!/bin/bash

set -e  # exit on error

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
