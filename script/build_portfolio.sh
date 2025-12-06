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

# Create output directory if missing
mkdir -p "$OUTPUT_DIR"

# Build the library
cd "$PORTFOLIO_DIR"
zig build

echo ""
echo "📁 Copying library to output folder..."

LIB_PATH="$PORTFOLIO_DIR/zig-out/lib/libzigPortfolio.dylib"

if [ -f "$LIB_PATH" ]; then
    cp "$LIB_PATH" "$OUTPUT_DIR/"
    echo "✅ Build complete. Library copied to:"
    echo "   $OUTPUT_DIR/libzigPortfolio.dylib"
else
    echo "❌ ERROR: Could not find compiled library:"
    echo "   $LIB_PATH"
    exit 1
fi

echo ""
echo "🧹 Cleaning up zig-out folder..."

# Extra safety check: zig-out must exist and be inside zigPortfolio
if [[ "$PORTFOLIO_DIR/zig-out" == "$PORTFOLIO_DIR/zig-out" && -d "$PORTFOLIO_DIR/zig-out" ]]; then
    rm -rf "$PORTFOLIO_DIR/zig-out"
    echo "✔ Deleted zig-out folder."
else
    echo "❌ Safety check failed: zig-out folder path is unexpected."
    exit 1
fi

echo ""
echo "🎉 Done!"
