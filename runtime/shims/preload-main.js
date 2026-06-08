// preload-main.js — Hub preload script for macOS Electron apps running on Linux
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0 Runtime Substitution
// Loaded via --preload when the Electron adapter executes the app.
//
// This is the SINGLE preload entry point. It conditionally activates sub-shims
// based on MACRUN_SHIM_* environment variables set by the Electron adapter.
//
// Sub-shims are required in order. Each checks its activation env var before
// patching any APIs. This keeps all shim behavior inspectable and deterministic.

(function activate_shims() {
    try {
        require('./env-normalizer');
    } catch (e) {
        console.error('[macrun-shim] env-normalizer failed:', e.message);
    }

    try {
        require('./platform-normalizer');
    } catch (e) {
        console.error('[macrun-shim] platform-normalizer failed:', e.message);
    }

    if (process.env.MACRUN_SHIM_PATHS === '1') {
        try {
            require('./path-mapper');
        } catch (e) {
            console.error('[macrun-shim] path-mapper failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_DISABLE_GPU === '1') {
        try {
            require('./disable-gpu');
        } catch (e) {
            console.error('[macrun-shim] disable-gpu failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_DISABLE_UPDATER === '1') {
        try {
            require('./disable-sparkle');
        } catch (e) {
            console.error('[macrun-shim] disable-sparkle failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_NOTIFICATIONS === '1') {
        try {
            require('./notification-bridge');
        } catch (e) {
            console.error('[macrun-shim] notification-bridge failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_CLIPBOARD === '1') {
        try {
            require('./clipboard-bridge');
        } catch (e) {
            console.error('[macrun-shim] clipboard-bridge failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_SHELL === '1') {
        try {
            require('./shell-integration');
        } catch (e) {
            console.error('[macrun-shim] shell-integration failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_NORMALIZATION === '1') {
        try {
            require('./electron-normalization-registry');
        } catch (e) {
            console.error('[macrun-shim] electron-normalization-registry failed:', e.message);
        }
    }

    if (process.env.MACRUN_DIAG_RENDERER === '1') {
        try {
            require('./renderer-diag');
        } catch (e) {
            console.error('[macrun-shim] renderer-diag failed:', e.message);
        }
    }

    if (process.env.MACRUN_SHIM_NATIVE_LOADER === '1') {
        try {
            require('./native-module-loader');
        } catch (e) {
            console.error('[macrun-shim] native-module-loader failed:', e.message);
        }
    }
})();
