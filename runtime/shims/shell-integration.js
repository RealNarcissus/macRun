// shell-integration.js — bridges shell.openExternal to xdg-open
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
// Activated by: MACRUN_SHIM_SHELL=1
//
// macOS apps use shell.openExternal() which on macOS opens URLs in the default
// browser or file manager. On Linux, we bridge to xdg-open which provides the
// same behavior (opens in the user's preferred application).
//
// Also bridges shell.showItemInFolder() to xdg-open with the parent directory.

(function bridge_shell() {
    if (process.env.MACRUN_SHIM_SHELL !== '1') return;

    const { spawn } = require('child_process');

    function open_external(url) {
        return new Promise(function (resolve) {
            if (!url || typeof url !== 'string') {
                resolve();
                return;
            }

            // Validate URL to prevent command injection
            if (!url.startsWith('http:') && !url.startsWith('https:') &&
                !url.startsWith('file:') && !url.startsWith('/') &&
                !url.startsWith('mailto:')) {
                resolve();
                return;
            }

            const child = spawn('xdg-open', [url], {
                stdio: 'ignore',
                detached: true
            });
            child.on('error', function () { resolve(); });
            child.on('close', function () { resolve(); });
            child.unref();
        });
    }

    function show_item_in_folder(fullPath) {
        // Open the parent directory in the file manager
        const path = require('path');
        const dir = path.dirname(fullPath);
        spawn('xdg-open', [dir], {
            stdio: 'ignore',
            detached: true
        }).unref();
    }

    try {
        const { shell } = require('electron');
        if (shell && !shell._macrun_patched) {
            const origOpenExternal = shell.openExternal.bind(shell);
            shell.openExternal = async function (url, options) {
                try {
                    return await origOpenExternal(url, options);
                } catch (_) {
                    await open_external(url);
                }
            };

            if (typeof shell.showItemInFolder === 'function') {
                const origShowItem = shell.showItemInFolder.bind(shell);
                shell.showItemInFolder = function (fullPath) {
                    try {
                        origShowItem(fullPath);
                    } catch (_) {
                        show_item_in_folder(fullPath);
                    }
                };
            }

            shell._macrun_patched = true;
        }
    } catch (_) {
        // shell not available
    }
})();
