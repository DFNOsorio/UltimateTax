#!/bin/bash

set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT_DIR="$ROOT_DIR/script"
PORTFOLIO_DIR="$ROOT_DIR/zigPortfolio"
FRONTEND_DIR="$ROOT_DIR/frontendConnector"
OUTPUT_DIR="$ROOT_DIR/build_output"
PORTFOLIO_LIB="$OUTPUT_DIR/libzigPortfolio.dylib"
RECOMPILE=false

# Parse flags
for arg in "$@"; do
    case $arg in
        --recompile|recompile=true)
            RECOMPILE=true
            ;;
    esac
done

echo "🚀 Running frontendConnector..."
echo "Root directory:             $ROOT_DIR"
echo "Library path:               $PORTFOLIO_LIB"
echo "Recompile requested:        $RECOMPILE"
echo ""

# Check if we need to build zigPortfolio
if [[ ! -f "$PORTFOLIO_LIB" || "$RECOMPILE" = true ]]; then
    echo "🔨 Building zigPortfolio..."
    "$SCRIPT_DIR/build_portfolio.sh"
else
    echo "✔ Using existing libzigPortfolio.dylib"
fi

echo ""
echo "🏗  Building frontendConnector..."
cd "$FRONTEND_DIR"
zig build

echo ""
echo "📁 Copying libzigPortfolio.dylib next to frontendConnector executable..."
mkdir -p "$FRONTEND_DIR/zig-out/bin"
cp "$PORTFOLIO_LIB" "$FRONTEND_DIR/zig-out/bin/"

echo ""
echo "▶️ Running frontendConnector..."
"$FRONTEND_DIR/zig-out/bin/frontendConnector"
