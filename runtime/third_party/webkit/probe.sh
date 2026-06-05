#!/bin/bash
# WebKitGTK system probe and integration check
# Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Section 3 (WebKitGTK)
#   Minimum version: 2.40.0 (see Section 7, Version Baselines)
#
# WebKitGTK is a system dependency installed via the host package manager.
# The adapter probes the system at runtime — never linked at compile time
# into the orchestrator.
#
# Usage: ./probe.sh

set -euo pipefail

echo "=== WebKitGTK Substrate Probe ==="
echo ""

MIN_VERSION="2.40.0"

check_pkgconfig() {
    if command -v pkg-config &>/dev/null; then
        if pkg-config --exists webkit2gtk-4.1 2>/dev/null; then
            local VERSION=$(pkg-config --modversion webkit2gtk-4.1)
            echo "  webkit2gtk-4.1: $VERSION (pkg-config)"
            pkg-config --cflags --libs webkit2gtk-4.1 2>/dev/null | head -1 || true
            return 0
        elif pkg-config --exists webkit2gtk-4.0 2>/dev/null; then
            local VERSION=$(pkg-config --modversion webkit2gtk-4.0)
            echo "  webkit2gtk-4.0: $VERSION (pkg-config)"
            return 0
        fi
    fi
    return 1
}

check_header() {
    for path in /usr/include/webkitgtk-4.1 /usr/include/webkitgtk-4.0 \
                /usr/local/include/webkitgtk-4.1; do
        if [ -f "$path/webkit2/webkit2.h" ]; then
            echo "  Headers found at: $path"
            return 0
        fi
    done
    return 1
}

check_library() {
    if ldconfig -p 2>/dev/null | grep -q libwebkit2gtk; then
        echo "  Shared library found:"
        ldconfig -p 2>/dev/null | grep libwebkit2gtk | head -3 || true
        return 0
    fi
    return 1
}

STATUS="available"
DETAILS=""

if check_pkgconfig; then
    DETAILS="pkg-config"
elif check_header && check_library; then
    DETAILS="headers+library (no pkg-config)"
else
    STATUS="unavailable"
    DETAILS="WebKitGTK development package not found"
fi

echo ""
echo "  Status: $STATUS"
echo "  Details: $DETAILS"
echo "  Minimum required: $MIN_VERSION"
echo ""

# Write probe result
cat > "$(dirname "$0")/webkit-probe.json" << EOF
{
  "substrate": "webkitgtk",
  "min_version": "$MIN_VERSION",
  "status": "$STATUS",
  "details": "$DETAILS",
  "probed_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "host_distribution": "$(lsb_release -ds 2>/dev/null || cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2 | tr -d '\"' || echo 'unknown')"
}
EOF

echo "  Probe result written to: webkit-probe.json"

if [ "$STATUS" = "unavailable" ]; then
    echo ""
    echo "  To install WebKitGTK on Ubuntu/Debian:"
    echo "    sudo apt install libwebkit2gtk-4.1-dev"
    echo ""
    echo "  On Fedora:"
    echo "    sudo dnf install webkit2gtk4.1-devel"
    echo ""
    echo "  On Arch:"
    echo "    sudo pacman -S webkit2gtk-6.0"
fi

echo "  Done."
