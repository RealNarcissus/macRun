// clipboard-bridge.js — bridges clipboard API to xclip / wl-clipboard
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
// Activated by: MACRUN_SHIM_CLIPBOARD=1
//
// Electron's clipboard API works on Linux via the system clipboard.
// This shim ensures clipboard operations fall back to xclip (X11) or
// wl-copy/wl-paste (Wayland) when Electron's built-in clipboard
// encounters issues with specific content types.

(function bridge_clipboard() {
    if (process.env.MACRUN_SHIM_CLIPBOARD !== '1') return;

    const { spawnSync, spawn } = require('child_process');
    const isWayland = !!process.env.WAYLAND_DISPLAY;

    function get_clipboard_text() {
        try {
            if (isWayland) {
                const result = spawnSync('wl-paste', ['--primary'], {
                    encoding: 'utf-8', timeout: 2000
                });
                if (result.status === 0) return result.stdout;
            } else {
                const result = spawnSync('xclip', ['-selection', 'clipboard', '-o'], {
                    encoding: 'utf-8', timeout: 2000
                });
                if (result.status === 0) return result.stdout;
            }
        } catch (_) {}
        return null;
    }

    function set_clipboard_text(text) {
        try {
            const cmd = isWayland ? 'wl-copy' : 'xclip';
            const args = isWayland ? [] : ['-selection', 'clipboard'];
            const child = spawn(cmd, args, {
                stdio: ['pipe', 'ignore', 'ignore']
            });
            child.stdin.write(text);
            child.stdin.end();
            child.unref();
            return true;
        } catch (_) {}
        return false;
    }

    try {
        const { clipboard } = require('electron');
        if (clipboard && !clipboard._macrun_patched) {
            const origRead = clipboard.readText.bind(clipboard);
            const origWrite = clipboard.writeText.bind(clipboard);

            clipboard.readText = function (type) {
                const result = origRead(type);
                if (result) return result;
                const fallback = get_clipboard_text();
                return fallback || result;
            };

            clipboard.writeText = function (text, type) {
                try {
                    return origWrite(text, type);
                } catch (_) {
                    return set_clipboard_text(text);
                }
            };

            clipboard._macrun_patched = true;
        }
    } catch (_) {
        // clipboard not available
    }
})();
