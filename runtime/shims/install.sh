#!/bin/bash
# install.sh — Install macOS Runtime Platform shim scripts
# Copies all .js shim files from the source directory to ~/.cache/macrun/shims/
# This script is called by the Electron acquisition script and during
# platform installation. Shims are inspectable .js files — not embedded in C++.
#
# Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0 Runtime Substitution

set -euo pipefail

SHIMS_DIR="$HOME/.cache/macrun/shims"
SOURCE_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== MacRun Shim Installation ==="
echo "  Source: $SOURCE_DIR"
echo "  Target: $SHIMS_DIR"

mkdir -p "$SHIMS_DIR"

SHIM_FILES=(
    "preload-main.js"
    "env-normalizer.js"
    "platform-normalizer.js"
    "path-mapper.js"
    "disable-sparkle.js"
    "disable-gpu.js"
    "notification-bridge.js"
    "clipboard-bridge.js"
    "shell-integration.js"
    "electron-normalization-registry.js"
    "native-module-loader.js"
    "renderer-diag.js"
    "main-diag.js"
    "esm-loader.mjs"
    "mock-diagnostics-channel.mjs"
    "mock-module.mjs"
)

copied=0
for shim in "${SHIM_FILES[@]}"; do
    if [ -f "$SOURCE_DIR/$shim" ]; then
        cp "$SOURCE_DIR/$shim" "$SHIMS_DIR/$shim"
        echo "  Installed: $shim"
        ((copied++)) || true
    else
        echo "  WARNING: Source shim not found: $shim"
    fi
done

echo "  Done. $copied shims installed to $SHIMS_DIR"
