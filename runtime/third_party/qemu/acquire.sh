#!/bin/bash
# QEMU user-mode substrate acquisition script
# Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Section 3 (QEMU User-Mode)
#   Pinned to release: 8.2.0 (static user-mode translation)
#
# Per SUBSTRATE_MODEL.md Section 9: QEMU static binaries must NOT be compiled
# from source on the fly during standard acquisition. Dynamic compilation leaks
# host library dependencies and breaks build isolation.
#
# Default behavior: probe system for pre-installed qemu-user-static package.
# --download: pull verified pre-built static binary with SHA256 integrity check.
# --build: explicitly opt-in to source compilation (isolated workspace, SHA256 verified tarball).
#
# Usage: ./acquire.sh [--download] [--build] [--arch x86_64|aarch64]

set -euo pipefail

QEMU_VERSION="8.2.0"
QEMU_SOURCE_URL="https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz"
QEMU_SOURCE_SHA256="e9eb31ebbb18af6004fb38bb2a8e2c8820ba7e01ff5666a9e38a45eaf79c25b6"
# IMPORTANT: The SHA256 above must be verified against the official QEMU release
# page (https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz.sha256 or equivalent).
# Update this hash whenever QEMU_VERSION changes.
# Last verified: placeholder — validate before production use.

ARCH="${ARCH:-x86_64}"
CACHE_DIR="$(cd "$(dirname "$0")" && pwd)/qemu-binaries"
TMP_DL_DIR="/tmp/macrun-qemu-$$"

MODE="probe"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --download) MODE="download"; shift ;;
        --build)   MODE="build"; shift ;;
        --arch)    ARCH="$2"; shift 2 ;;
        --cache-dir) CACHE_DIR="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "=== QEMU User-Mode Substrate Acquisition ==="
echo "  Version: $QEMU_VERSION"
echo "  Arch:    $ARCH"
echo "  Mode:    $MODE"
echo ""

cleanup() {
    rm -rf "$TMP_DL_DIR" 2>/dev/null || true
}
trap cleanup EXIT

# -------------------------------------------------------
# Mode: probe — check system for pre-installed binaries
# -------------------------------------------------------
probe_system() {
    echo "  Probing system for QEMU user-mode static binaries..."

    for candidate in qemu-${ARCH} qemu-${ARCH}-static; do
        local found
        found=$(command -v "$candidate" 2>/dev/null || true)
        if [ -n "$found" ]; then
            local ver
            ver=$("$found" --version 2>&1 | head -1 || true)
            echo "  Found: $found"
            echo "  Version: $ver"
            # Symlink into cache for lifecycle manager lookup
            mkdir -p "$CACHE_DIR"
            ln -sf "$found" "$CACHE_DIR/$candidate"
            write_manifest "$found"
            return 0
        fi
    done

    echo "  No system QEMU user-mode binary found."
    echo "  Install via package manager:"
    echo "    Debian/Ubuntu: sudo apt install qemu-user-static"
    echo "    Fedora:        sudo dnf install qemu-user-static"
    echo "    Arch:          sudo pacman -S qemu-user-static"
    echo ""
    echo "  Or use: $0 --download"
    echo "  Or use: $0 --build"
    return 1
}

# -------------------------------------------------------
# Mode: download — pull pre-built static binary
# -------------------------------------------------------
download_binary() {
    echo "  Downloading pre-built QEMU user-mode static binary ($QEMU_VERSION, $ARCH)..."

    # QEMU official project does not publish pre-compiled user-mode static
    # binaries. We pull from the Debian package archive as a reproducible source.
    # The binary is verified via SHA256 after extraction.
    #
    # Architecture mapping:
    STATIC_NAME="qemu-${ARCH}-static"

    if [ -f "$CACHE_DIR/$STATIC_NAME" ]; then
        echo "  Already cached: $CACHE_DIR/$STATIC_NAME"
        if "$CACHE_DIR/$STATIC_NAME" --version 2>&1 | grep -q "$QEMU_VERSION"; then
            write_manifest "$CACHE_DIR/$STATIC_NAME"
            return 0
        fi
        echo "  Cached binary version mismatch. Re-downloading..."
        rm -f "$CACHE_DIR/$STATIC_NAME"
    fi

    # Strategy: extract qemu-*-static from distro package archive.
    # Debian trixie (testing) carries qemu-user-static 8.2.x.
    # If the package architecture changes, update DEB_URL and DEB_SHA256 below.
    local DEB_URL
    local DEB_SHA256
    local DEB_FILENAME

    case "$ARCH" in
        x86_64)
            # amd64 host, qemu-user-static package for translating other archs
            DEB_URL="http://ftp.debian.org/debian/pool/main/q/qemu/qemu-user-static_${QEMU_VERSION}-1_amd64.deb"
            DEB_SHA256="placeholder-verify-before-use"
            DEB_FILENAME="qemu-user-static_${QEMU_VERSION}-1_amd64.deb"
            ;;
        aarch64)
            DEB_URL="http://ftp.debian.org/debian/pool/main/q/qemu/qemu-user-static_${QEMU_VERSION}-1_arm64.deb"
            DEB_SHA256="placeholder-verify-before-use"
            DEB_FILENAME="qemu-user-static_${QEMU_VERSION}-1_arm64.deb"
            ;;
        *)
            echo "  ERROR: Unsupported architecture: $ARCH"
            return 1
            ;;
    esac

    mkdir -p "$CACHE_DIR" "$TMP_DL_DIR"

    echo "  Downloading: $DEB_URL"
    if ! curl -sL --fail --retry 3 -o "$TMP_DL_DIR/$DEB_FILENAME" "$DEB_URL"; then
        echo "  ERROR: Download failed. Network or URL issue: $DEB_URL"
        return 1
    fi

    # SHA256 integrity verification
    echo "  Verifying SHA256..."
    local COMPUTED
    COMPUTED=$(sha256sum "$TMP_DL_DIR/$DEB_FILENAME" | cut -d' ' -f1)
    if [ "$COMPUTED" != "$DEB_SHA256" ]; then
        echo "  ERROR: SHA256 mismatch!"
        echo "    Expected: $DEB_SHA256"
        echo "    Got:      $COMPUTED"
        echo "  The downloaded file is corrupt or has been tampered with."
        echo "  Aborting for safety."
        rm -f "$TMP_DL_DIR/$DEB_FILENAME"
        return 1
    fi
    echo "  SHA256 verified: $COMPUTED"

    # Extract the static binary from the .deb
    echo "  Extracting static binary from package..."
    mkdir -p "$TMP_DL_DIR/extracted"
    dpkg-deb -x "$TMP_DL_DIR/$DEB_FILENAME" "$TMP_DL_DIR/extracted" 2>/dev/null || {
        # dpkg-deb unavailable? fall back to ar + tar
        cd "$TMP_DL_DIR"
        ar x "$DEB_FILENAME" data.tar.xz 2>/dev/null || {
            echo "  ERROR: Cannot extract .deb. Install dpkg-deb or binutils."
            return 1
        }
        tar xf data.tar.xz -C extracted
    }

    # Find the qemu-*-static binary in the extracted tree
    BIN_PATH=$(find "$TMP_DL_DIR/extracted" -name "$STATIC_NAME" -type f 2>/dev/null | head -1)
    if [ -z "$BIN_PATH" ]; then
        echo "  ERROR: $STATIC_NAME not found in downloaded package."
        return 1
    fi

    cp "$BIN_PATH" "$CACHE_DIR/$STATIC_NAME"
    chmod +x "$CACHE_DIR/$STATIC_NAME"

    echo "  QEMU static binary installed to: $CACHE_DIR/$STATIC_NAME"
    "$CACHE_DIR/$STATIC_NAME" --version 2>&1 | head -1 || true

    write_manifest "$CACHE_DIR/$STATIC_NAME"
}

# -------------------------------------------------------
# Mode: build — explicit opt-in source compilation
# -------------------------------------------------------
build_from_source() {
    echo "  WARNING: Building QEMU from source is an explicit opt-in."
    echo "  This operation will consume significant CPU and disk resources."
    echo "  Build isolation: compiled in isolated $TMP_DL_DIR workspace."
    echo ""

    if [ -f "$CACHE_DIR/qemu-${ARCH}" ] || [ -f "$CACHE_DIR/qemu-${ARCH}-static" ]; then
        echo "  Binary already cached. Use 'rm $CACHE_DIR/qemu-*' to force rebuild."
        write_manifest "$CACHE_DIR/qemu-${ARCH}"
        return 0
    fi

    mkdir -p "$TMP_DL_DIR"
    QEMU_TARBALL="qemu-${QEMU_VERSION}.tar.xz"

    if ! curl -sL --fail --retry 3 -o "$TMP_DL_DIR/$QEMU_TARBALL" "$QEMU_SOURCE_URL"; then
        echo "  ERROR: Source download failed: $QEMU_SOURCE_URL"
        return 1
    fi

    # SHA256 verification of source tarball
    echo "  Verifying source tarball SHA256..."
    local COMPUTED
    COMPUTED=$(sha256sum "$TMP_DL_DIR/$QEMU_TARBALL" | cut -d' ' -f1)
    if [ "$COMPUTED" != "$QEMU_SOURCE_SHA256" ]; then
        echo "  ERROR: Source tarball SHA256 mismatch!"
        echo "    Expected: $QEMU_SOURCE_SHA256"
        echo "    Got:      $COMPUTED"
        echo "  Aborting for safety."
        rm -f "$TMP_DL_DIR/$QEMU_TARBALL"
        return 1
    fi
    echo "  Source SHA256 verified."

    BUILD_DIR="$TMP_DL_DIR/qemu-build-${QEMU_VERSION}"
    echo "  Extracting source to isolated build directory..."
    mkdir -p "$BUILD_DIR"
    tar xf "$TMP_DL_DIR/$QEMU_TARBALL" -C "$BUILD_DIR" --strip-components=1

    echo "  Configuring QEMU user-mode static build..."
    cd "$BUILD_DIR"
    ./configure --target-list=${ARCH}-linux-user --static --disable-system \
        --disable-tools --disable-guest-agent --disable-werror 2>&1 | tail -3

    echo "  Building (parallel: $(nproc) jobs)..."
    make -j"$(nproc)" 2>&1 | tail -3

    STATIC_NAME="qemu-${ARCH}"
    if [ -f "build/${ARCH}-linux-user/$STATIC_NAME" ]; then
        mkdir -p "$CACHE_DIR"
        cp "build/${ARCH}-linux-user/$STATIC_NAME" "$CACHE_DIR/$STATIC_NAME"
        echo "  QEMU static binary installed to: $CACHE_DIR/$STATIC_NAME"
        "$CACHE_DIR/$STATIC_NAME" --version 2>&1 | head -1 || true
        write_manifest "$CACHE_DIR/$STATIC_NAME"
    else
        echo "  ERROR: Build produced no binary at expected path."
        return 1
    fi
}

# -------------------------------------------------------
# Manifest writer
# -------------------------------------------------------
write_manifest() {
    local binary_path="$1"
    cat > "$(dirname "$0")/qemu-version.txt" << EOF
substrate=qemu-user
version=$QEMU_VERSION
arch=$ARCH
binary=$binary_path
acquired=$(date -u +%Y-%m-%dT%H:%M:%SZ)
mode=$MODE
EOF
    echo "  Version manifest written to qemu-version.txt"
}

# -------------------------------------------------------
# Dispatch
# -------------------------------------------------------
case "$MODE" in
    probe)    probe_system;;
    download) download_binary;;
    build)    build_from_source;;
    *) echo "Unknown mode: $MODE"; exit 1;;
esac

echo "  Done."
