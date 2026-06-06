#!/bin/bash
# Electron runtime acquisition and cache management
# Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Section 3 (Electron Runtimes)
#   Pinned matrix: 22.x, 24.x, 28.x (see SUBSTRATE_MODEL.md Section 7, Version Baselines)
#
# Electron runtimes are NEVER compiled. Pre-packaged native Linux Electron
# binaries are downloaded from GitHub releases and verified via SHA256.
#
# Cache location: ~/.cache/macrun/electron/
#
# Usage: ./acquire.sh [--version 28.3.3] [--all] [--dry-run]

set -euo pipefail

CACHE_DIR="$HOME/.cache/macrun/electron"
MANIFEST_DIR="$(cd "$(dirname "$0")/../../.." && pwd)/compat-db/manifests/electron"
TMP_DL_DIR="/tmp/macrun-electron-$$"

# Electron releases: https://github.com/electron/electron/releases
# SHA256 hashes from official SHASUMS256.txt files at release time.
# Update these when version pins change.
declare -A ELECTRON_SHA256=(
    ["22.3.27"]="631d8eb08098c48ce2b29421e74c69ac0312b1e42f445d8a805414ba1242bf3a"
    ["24.8.8"]="f4604c0f0f346787abdbc364cac1e488441a2d21068ecbcf53052ed6ffb0309b"
    ["28.3.3"]="20f6be493cbd6c9924206e744b1c490af1f97f4735451b9dc19f0d305366d546"
    ["41.7.1"]="0165fc68656f49ad7ae0c4254b1ff3af1718c114b6007a2aeef5211e0562f174"
    ["42.3.3"]="bcc22137758607216d764dc453e2fea454c808793c3d6a82ddf86a36f9defa2c"
)

ELECTRON_VERSIONS=("22.3.27" "24.8.8" "28.3.3" "41.7.1" "42.3.3")
ACQUIRE_ALL=false
DRY_RUN=false
VERIFY_ONLY=false
TARGET_VERSION=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)   TARGET_VERSION="$2"; shift 2 ;;
        --all)       ACQUIRE_ALL=true; shift ;;
        --dry-run)   DRY_RUN=true; shift ;;
        --verify-only) VERIFY_ONLY=true; shift ;;
        --cache-dir) CACHE_DIR="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "=== Electron Runtime Acquisition ==="
echo "  Cache: $CACHE_DIR"
echo ""

cleanup() {
    rm -rf "$TMP_DL_DIR" 2>/dev/null || true
}
trap cleanup EXIT

# -------------------------------------------------------
# Download + verify + cache a single Electron version
# -------------------------------------------------------
acquire_version() {
    local VERSION="$1"
    local CACHED_BINARY="$CACHE_DIR/electron-$VERSION/electron"
    local ZIP_NAME="electron-v${VERSION}-linux-x64.zip"
    local DOWNLOAD_URL="https://github.com/electron/electron/releases/download/v${VERSION}/${ZIP_NAME}"
    local EXPECTED_SHA256="${ELECTRON_SHA256[$VERSION]}"

    if [ -f "$CACHED_BINARY" ]; then
        echo "  Electron $VERSION already cached at: $CACHED_BINARY"
        return 0
    fi

    if [ "$DRY_RUN" = true ]; then
        echo "  [DRY RUN] Would download Electron $VERSION from:"
        echo "    $DOWNLOAD_URL"
        echo "  [DRY RUN] SHA256 expected: $EXPECTED_SHA256"
        return 0
    fi

    mkdir -p "$CACHE_DIR/electron-$VERSION" "$TMP_DL_DIR"

    echo "  Downloading Electron $VERSION..."
    if ! curl -sL --fail --retry 3 -o "$TMP_DL_DIR/$ZIP_NAME" "$DOWNLOAD_URL"; then
        echo "  ERROR: Download failed for Electron $VERSION"
        echo "  URL: $DOWNLOAD_URL"
        return 1
    fi

    # SHA256 integrity verification
    echo "  Verifying SHA256..."
    local COMPUTED
    COMPUTED=$(sha256sum "$TMP_DL_DIR/$ZIP_NAME" | cut -d' ' -f1)

    if [ "$COMPUTED" != "$EXPECTED_SHA256" ]; then
        echo "  ERROR: SHA256 mismatch for Electron $VERSION!"
        echo "    Expected: $EXPECTED_SHA256"
        echo "    Got:      $COMPUTED"
        echo "  The downloaded file is corrupt or has been tampered with."
        echo "  Aborting for safety."
        rm -f "$TMP_DL_DIR/$ZIP_NAME"
        return 1
    else
        echo "  SHA256 verified: $COMPUTED"
    fi

    # Extract the entire contents of the zip to the cache directory
    echo "  Extracting Electron archive..."
    if command -v unzip >/dev/null 2>&1; then
        unzip -qo "$TMP_DL_DIR/$ZIP_NAME" -d "$CACHE_DIR/electron-$VERSION"
    else
        echo "  ERROR: 'unzip' not found. Install it to extract Electron archives."
        return 1
    fi

    if [ ! -f "$CACHED_BINARY" ]; then
        echo "  ERROR: 'electron' binary not found after extraction."
        return 1
    fi
    chmod +x "$CACHED_BINARY"

    # Compute and store the binary hash for cache verification
    local BINARY_HASH
    BINARY_HASH=$(sha256sum "$CACHED_BINARY" | cut -d' ' -f1)

    echo "  Electron $VERSION cached at: $CACHED_BINARY"

    # Write per-version manifest with both zip and binary integrity
    cat > "$CACHE_DIR/electron-$VERSION/manifest.json" << MANIFEST
{
  "substrate": "electron",
  "version": "$VERSION",
  "platform": "linux-x64",
  "cached": true,
  "cache_path": "$CACHE_DIR/electron-$VERSION",
  "acquired": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "download_url": "$DOWNLOAD_URL",
  "integrity": {
    "algorithm": "sha256",
    "source_expected": "$EXPECTED_SHA256",
    "source_verified": "$COMPUTED",
    "binary_hash": "$BINARY_HASH"
  }
}
MANIFEST

    echo "  Manifest written with integrity record."
}

# -------------------------------------------------------
# Verify cached binaries against pinned SHA256 hashes
# -------------------------------------------------------
verify_cached() {
    echo "  Verifying cached Electron binaries..."
    local all_ok=true

    for ver in "${ELECTRON_VERSIONS[@]}"; do
        local CACHED_BINARY="$CACHE_DIR/electron-$ver/electron"
        local MANIFEST="$CACHE_DIR/electron-$ver/manifest.json"

        if [ ! -f "$CACHED_BINARY" ]; then
            echo "  MISSING: Electron $ver not cached"
            all_ok=false
            continue
        fi

        if [ ! -f "$MANIFEST" ]; then
            echo "  WARN:    Electron $ver — no manifest, can't verify"
            all_ok=false
            continue
        fi

        # Read the binary hash from the per-version manifest
        local EXPECTED_BINARY_HASH
        EXPECTED_BINARY_HASH=$(grep -o '"binary_hash": "[^"]*"' "$MANIFEST" 2>/dev/null | head -1 | sed 's/.*"binary_hash": "\([^"]*\)".*/\1/')

        if [ -z "$EXPECTED_BINARY_HASH" ] || [ "$EXPECTED_BINARY_HASH" = "null" ]; then
            echo "  WARN:    Electron $ver — no binary_hash in manifest"
            all_ok=false
            continue
        fi

        local COMPUTED
        COMPUTED=$(sha256sum "$CACHED_BINARY" | cut -d' ' -f1)

        if [ "$COMPUTED" = "$EXPECTED_BINARY_HASH" ]; then
            echo "  OK:      Electron $ver"
        else
            echo "  FAIL:    Electron $ver — SHA256 mismatch!"
            echo "               Expected: $EXPECTED_BINARY_HASH"
            echo "               Got:      $COMPUTED"
            all_ok=false
        fi
    done

    if [ "$all_ok" = true ]; then
        echo "  All cached binaries verified."
    else
        echo "  Some binaries failed verification. Re-run with --all to re-download."
        return 1
    fi
}

# -------------------------------------------------------
# Install shim scripts alongside runtime cache
# -------------------------------------------------------
install_shims() {
    local SHIMS_SOURCE="$(cd "$(dirname "$0")/../.." && pwd)/shims"
    if [ -x "$SHIMS_SOURCE/install.sh" ]; then
        echo ""
        echo "  Installing shim scripts..."
        bash "$SHIMS_SOURCE/install.sh"
    fi
}

# -------------------------------------------------------
# Dispatch
# -------------------------------------------------------

if [ "$VERIFY_ONLY" = true ]; then
    verify_cached
    exit $?
fi

if [ -n "$TARGET_VERSION" ]; then
    acquire_version "$TARGET_VERSION"
elif [ "$ACQUIRE_ALL" = true ]; then
    for ver in "${ELECTRON_VERSIONS[@]}"; do
        acquire_version "$ver" || echo "  Skipping $ver due to error."
    done
else
    echo "  Usage: $0 [--version 28.3.3] [--all] [--dry-run]"
    echo "  Pinned versions: ${ELECTRON_VERSIONS[*]}"
    echo ""
    echo "  Modes:"
    echo "    --version X.Y.Z   Acquire a specific Electron version"
    echo "    --all             Acquire all pinned versions"
    echo "    --dry-run         Show what would be downloaded"
    exit 1
fi

# Write the master version manifest for compat-db
mkdir -p "$MANIFEST_DIR"
cat > "$MANIFEST_DIR/electron-versions.json" << 'VEOF'
{
  "schema_version": "1.0.0",
  "description": "Electron runtime version mapping for macOS → Linux runtime substitution",
  "architecture_reference": "docs/architecture/SUBSTRATE_MODEL.md Section 3 (Electron Runtimes)",
  "pinned_versions": {
    "22.x": {
      "exact": "22.3.27",
      "status": "supported",
      "macos_equivalent": "Electron 22",
      "cache_path": "~/.cache/macrun/electron/electron-22.3.27"
    },
    "24.x": {
      "exact": "24.8.8",
      "status": "supported",
      "macos_equivalent": "Electron 24",
      "cache_path": "~/.cache/macrun/electron/electron-24.8.8"
    },
    "28.x": {
      "exact": "28.3.3",
      "status": "recommended",
      "macos_equivalent": "Electron 28",
      "cache_path": "~/.cache/macrun/electron/electron-28.3.3"
    },
    "41.x": {
      "exact": "41.7.1",
      "status": "supported",
      "macos_equivalent": "Electron 41",
      "cache_path": "~/.cache/macrun/electron/electron-41.7.1"
    },
    "42.x": {
      "exact": "42.3.3",
      "status": "supported",
      "macos_equivalent": "Electron 42",
      "cache_path": "~/.cache/macrun/electron/electron-42.3.3"
    }
  },
  "integrity": {
    "algorithm": "sha256",
    "verification": "on-acquire",
    "cache_max_age_days": 90
  },
  "discovery_contract": {
    "lookup_key": "CFBundleIdentifier",
    "version_detection": "Electron Framework.framework version string",
    "fallback": "latest-compatible-cached"
  }
}
VEOF

echo ""
echo "  Electron version manifest written to: $MANIFEST_DIR/electron-versions.json"

# Install shim scripts alongside runtimes
install_shims

echo "  Done."
