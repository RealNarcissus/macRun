// disable-sparkle.js — suppresses macOS auto-updater frameworks
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
// Activated by: MACRUN_SHIM_DISABLE_UPDATER=1
//
// macOS Electron apps often bundle Sparkle.framework or Squirrel for auto-updates.
// On Linux these frameworks don't exist and the app will crash or show errors
// when trying to check for updates. This shim monkey-patches the most common
// auto-updater patterns to no-op, preventing startup crashes and error dialogs.

(function disable_sparkle() {
    if (process.env.MACRUN_SHIM_DISABLE_UPDATER !== '1') return;

    try {
        const { autoUpdater } = require('electron');
        if (autoUpdater && !autoUpdater._macrun_patched) {
            const noopMethods = [
                'setFeedURL', 'checkForUpdates', 'quitAndInstall',
                'downloadUpdate', 'checkForUpdatesAndNotify'
            ];
            for (const method of noopMethods) {
                if (typeof autoUpdater[method] === 'function') {
                    autoUpdater[method] = function () {};
                }
            }

            try {
                autoUpdater.removeAllListeners('update-downloaded');
                autoUpdater.removeAllListeners('error');
                autoUpdater.on('error', function () {
                    // silently swallow updater errors
                });
            } catch (_) {}

            autoUpdater._macrun_patched = true;
        }
    } catch (_) {
        // autoUpdater not available — safe to ignore
    }

    // Suppress common update-related native module loads
    // Node's process.dlopen returns undefined and throws on error.
    // Returning module breaks Node's internal module loading contract.
    if (typeof process.dlopen === 'function') {
        var origDlopen = process.dlopen;
        var blockedModules = new Set([
            'sparkle', 'squirrel', 'Squirrel', 'updater',
            'electron-updater', 'electron-simple-updater'
        ]);
        process.dlopen = function (module, filename, flags) {
            var base = filename.toLowerCase();
            for (var _i = 0, _a = Array.from(blockedModules); _i < _a.length; _i++) {
                if (base.indexOf(_a[_i]) !== -1) {
                    // Conform to Node's dlopen contract: return undefined (void).
                    // Do NOT return module — that breaks Node's module loader
                    // and causes uncaught TypeError crashes in downstream scripts.
                    console.warn('[macrun-shim] Blocked native module load:', filename);
                    // Module.exports is mocked with empty stubs to prevent
                    // 'Cannot read property of undefined' errors if the app
                    // tries to use the blocked module's exports.
                    module.exports = {};
                    return;
                }
            }
            return origDlopen.call(this, module, filename, flags);
        };
    }
})();
