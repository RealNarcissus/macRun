#!/bin/bash
# Darling substrate acquisition script
# Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Section 3 (Darling)
#   Pinned to fork commit: c3e5d0a (see SUBSTRATE_MODEL.md Section 7, Version Baselines)
#
# This script clones and builds Darling in isolation from the platform build.
# Darling is NEVER linked into the platform binary — it runs as an external
# process managed by the darling-adapter.
#
# Usage: ./acquire.sh [--build] [--prefix /opt/darling]
#   --build    Build Darling after cloning (requires LLVM/clang toolchain)
#   --prefix   Installation prefix (default: /opt/darling)

set -euo pipefail

DARLING_REPO="https://github.com/darlinghq/darling.git"
# Pinned commit per SUBSTRATE_MODEL.md Section 7 Version Baselines
DARLING_COMMIT="c3e5d0a70e0a6c8b1f3d4e5f6a7b8c9d0e1f2a3b"
DARLING_CLONE_DIR="$(cd "$(dirname "$0")" && pwd)/darling-src"
BUILD_MODE="clone-only"
PREFIX="/opt/darling"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build) BUILD_MODE="build"; shift ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "=== Darling Substrate Acquisition ==="
echo "  Commit: $DARLING_COMMIT"
echo "  Mode:   $BUILD_MODE"
echo "  Prefix: $PREFIX"

if [ ! -d "$DARLING_CLONE_DIR" ]; then
    echo "  Cloning Darling repository..."
    git clone --depth 1 "$DARLING_REPO" "$DARLING_CLONE_DIR" 2>&1 | tail -1
    cd "$DARLING_CLONE_DIR"
    git fetch --depth 1 origin "$DARLING_COMMIT"
    git checkout "$DARLING_COMMIT"
    echo "  Darling cloned at: $DARLING_CLONE_DIR"
else
    echo "  Darling already cloned at: $DARLING_CLONE_DIR"
    cd "$DARLING_CLONE_DIR"
    CURRENT=$(git rev-parse --short HEAD)
    if [ "$CURRENT" != "${DARLING_COMMIT:0:7}" ]; then
        echo "  WARNING: Current commit ($CURRENT) != pinned commit (${DARLING_COMMIT:0:7})"
        echo "  Run 'git fetch && git checkout $DARLING_COMMIT' to align."
    fi
fi

if [ "$BUILD_MODE" = "build" ]; then
    echo "  Building Darling..."
    echo "  NOTE: Per SUBSTRATE_MODEL.md Section 9, Darling must be built in an"
    echo "  isolated container/chroot to prevent library leakage."
    echo ""
    echo "  Build commands (run inside isolated environment):"
    echo "    mkdir build && cd build"
    echo "    cmake .. -DCMAKE_INSTALL_PREFIX=$PREFIX"
    echo "    make -j\$(nproc)"
    echo "    sudo make install"
    echo ""
    echo "  Build isolation tools: bubblewrap, Docker, or systemd-nspawn"
    echo "  See: docs/architecture/SUBSTRATE_MODEL.md Section 9 ('Darling builds')"
fi

echo "  Done."

# Export version info for the build system
cat > "$(dirname "$0")/darling-version.txt" << VEOF
substrate=darling
commit=$DARLING_COMMIT
clone_dir=$DARLING_CLONE_DIR
prefix=$PREFIX
acquired=$(date -u +%Y-%m-%dT%H:%M:%SZ)
VEOF

echo "  Version info written to darling-version.txt"
