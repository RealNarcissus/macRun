// path-mapper.js — maps macOS app data paths to XDG directories
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
// Activated by: MACRUN_SHIM_PATHS=1
//
// Electron's app.getPath() returns macOS paths for .asar files built on macOS.
// This shim intercepts app.getPath() to return Linux XDG paths instead.
// path.resolve() calls involving ~/Library are also normalized.

(function map_paths() {
    if (process.env.MACRUN_SHIM_PATHS !== '1') return;

    const os = require('os');

    function resolve_xdg_path(name) {
        const home = process.env.HOME || os.homedir();
        const envMap = {
            'home':      () => home,
            'temp':      () => process.env.TMPDIR || '/tmp',
            'downloads': () => process.env.XDG_DOWNLOAD_DIR || home + '/Downloads',
            'documents': () => process.env.XDG_DOCUMENTS_DIR || home + '/Documents',
            'pictures':  () => process.env.XDG_PICTURES_DIR  || home + '/Pictures',
            'music':     () => process.env.XDG_MUSIC_DIR     || home + '/Music',
            'videos':    () => process.env.XDG_VIDEOS_DIR    || home + '/Videos',
            'desktop':   () => process.env.XDG_DESKTOP_DIR   || home + '/Desktop',
            'appData':   () => process.env.XDG_DATA_HOME     || home + '/.local/share',
            'userData':  () => process.env.XDG_CONFIG_HOME   || home + '/.config',
            'cache':     () => process.env.XDG_CACHE_HOME    || home + '/.cache',
            'sessionData': () => process.env.XDG_RUNTIME_DIR || '/tmp/macrun-session-' + (process.pid || 0),
            'exe':       () => process.env.MACRUN_APP_EXE || process.execPath,
            'module':    () => home + '/.local/share/macrun/modules',
            'crashDumps': () => process.env.XDG_CACHE_HOME  || home + '/.cache',
        };
        const fn = envMap[name];
        return fn ? fn() : null;
    }

    try {
        const { app } = require('electron');
        if (app && !app._macrun_paths_patched) {
            const origGetPath = app.getPath.bind(app);
            app.getPath = function (name) {
                const xdg = resolve_xdg_path(name);
                if (xdg) return xdg;
                return origGetPath(name);
            };
            app.getPath.resolve_xdg_path = resolve_xdg_path;
            app._macrun_paths_patched = true;
        }
    } catch (_) { /* app not available */ }

    // Set PATH to include common Linux locations
    if (process.env.PATH) {
        const extraPaths = ['/usr/local/bin', os.homedir() + '/.local/bin'];
        const existing = new Set(process.env.PATH.split(':'));
        for (const p of extraPaths) {
            if (!existing.has(p)) {
                process.env.PATH = p + ':' + process.env.PATH;
                existing.add(p);
            }
        }
    }
})();
