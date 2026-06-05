// disable-gpu.js — disables hardware GPU acceleration
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
// Activated by: MACRUN_SHIM_DISABLE_GPU=1
//
// Linux Electron can struggle with GPU compositing on some systems.
// This shim disables GPU acceleration early in app startup to ensure
// the app renders correctly via software rasterization.

(function disable_gpu() {
    if (process.env.MACRUN_SHIM_DISABLE_GPU !== '1') return;

    // Set flags before the GPU process initializes
    try {
        const { app } = require('electron');
        if (app) {
            app.commandLine.appendSwitch('disable-gpu');
            app.commandLine.appendSwitch('disable-gpu-compositing');
            app.commandLine.appendSwitch('disable-software-rasterizer');
            // Disable GPU sandbox to avoid process creation failures
            app.commandLine.appendSwitch('disable-gpu-sandbox');

            if (!app._macrun_gpu_disabled) {
                app.whenReady().then(function () {
                    try {
                        app.disableHardwareAcceleration();
                    } catch (_) {
                        // may throw if called too late; flags handle it
                    }
                });
                app._macrun_gpu_disabled = true;
            }
        }
    } catch (_) {
        // app not available
    }
})();
