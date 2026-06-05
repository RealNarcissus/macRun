// main-diag.js — Main-process Electron lifecycle diagnostic (injected via NODE_OPTIONS=--require)
// Architecture: docs/architecture/DEGRADATION_MODEL.md
// Activated by: MACRUN_DIAG_MAIN=1
//
// This module instruments the Electron main process lifecycle events.
// It is PURELY OBSERVATIONAL — never mutates application behavior.

(function () {
    if (process.env.MACRUN_DIAG_MAIN !== '1') return;

    // Prevent uncaught exceptions from broken pipe (EIO) on stdout/stderr
    if (process.stdout && typeof process.stdout.on === 'function') {
        process.stdout.on('error', function () {});
    }
    if (process.stderr && typeof process.stderr.on === 'function') {
        process.stderr.on('error', function () {});
    }

    var diagFile = process.env.MACRUN_DIAG_FILE || '/tmp/macrun-diag.log';
    var fs = require('fs');
    var pid = process.pid;

    function ts() { return new Date().toISOString(); }
    function writeDiag(category, event, detail) {
        var line = ts() + ' [main pid=' + pid + '] [MACRUN:DIAG:' + category + '] ' + event;
        if (detail) {
            try { line += ' ' + JSON.stringify(detail); } catch (_) { line += ' ' + String(detail); }
        }
        line += '\n';
        try { fs.appendFileSync(diagFile, line); } catch (_) {}
        try { process.stderr.write('[diag] ' + line); } catch (_) {}
    }

    function classifyError(error) {
        if (!error) return 'unknown';
        var msg = error.message || String(error);
        if (msg.indexOf('Cannot find') !== -1) return 'module_resolution';
        if (msg.indexOf('not defined') !== -1) return 'missing_api';
        if (msg.indexOf('protocol') !== -1) return 'protocol';
        if (msg.indexOf('GPU') !== -1) return 'gpu';
        return 'runtime';
    }

    writeDiag('lifecycle', 'main_diag_active', {argv: process.argv.slice(0, 4)});

    // ---- Electron lifecycle hooks ----
    try {
        var Module = require('module');
        var originalLoad = Module._load;
        var hooksApplied = false;

        function applyElectronHooks(electron) {
            if (hooksApplied) return;
            hooksApplied = true;

            var app = electron.app;
            var origBW = electron.BrowserWindow;
            var windowCount = 0;

            if (!app || !origBW) {
                writeDiag('preload', 'main_diag_electron_components_missing', {
                    hasApp: !!app,
                    hasBrowserWindow: !!origBW
                });
                return;
            }

            // ---- app events ----
            var appEvents = ['ready', 'window-all-closed', 'before-quit', 'will-quit', 'quit',
                             'activate', 'open-file', 'open-url', 'certificate-error',
                             'select-client-certificate', 'login', 'gpu-process-crashed',
                             'render-process-gone', 'child-process-gone', 'accessibility-support-changed'];
            appEvents.forEach(function (evt) {
                try {
                    app.on(evt, function () {
                        var detail = {};
                        if (evt === 'render-process-gone' || evt === 'gpu-process-crashed') {
                            detail = arguments[1] || {};
                        }
                        writeDiag('app', 'app.' + evt, detail);
                    });
                } catch (_) {}
            });

            // ---- BrowserWindow lifecycle ----
            var PatchedBrowserWindow = function (options) {
                windowCount++;
                var id = windowCount;
                writeDiag('window', 'BrowserWindow_created', {id: id, options: options ? Object.keys(options || {}) : []});

                var win = new origBW(options || {});

                // ---- Event instrumentation ----
                var winEvents = [
                    'ready-to-show', 'show', 'hide', 'close', 'closed',
                    'focus', 'blur', 'maximize', 'unmaximize', 'minimize', 'restore',
                    'resize', 'move', 'enter-full-screen', 'leave-full-screen',
                    'page-title-updated'
                ];
                winEvents.forEach(function (evt) {
                    try {
                        win.on(evt, function () {
                            writeDiag('window', 'win#' + id + '.' + evt, {id: id});
                        });
                    } catch (_) {}
                });

                // ---- webContents lifecycle ----
                if (win.webContents) {
                    var wc = win.webContents;
                    var wcEvents = [
                        'did-start-loading', 'did-stop-loading',
                        'did-finish-load', 'did-fail-load',
                        'did-create-window', 'will-navigate', 'did-navigate',
                        'dom-ready', 'render-process-gone',
                        'crashed', 'unresponsive', 'responsive',
                        'plugin-crashed', 'destroyed',
                        'select-bluetooth-device', 'paint',
                        'devtools-opened', 'devtools-closed',
                        'certificate-error', 'context-menu',
                        'zoom-changed', 'cursor-changed',
                        'media-started-playing', 'media-paused',
                        'update-target-url', 'new-window',
                        'will-redirect', 'did-redirect-navigation',
                        'login', 'found-in-page'
                    ];
                    wcEvents.forEach(function (evt) {
                        try {
                            wc.on(evt, function (event) {
                                var detail = {id: id};
                                if (evt === 'did-fail-load') {
                                    detail.errorCode = arguments[1];
                                    detail.errorDescription = arguments[2];
                                    detail.validatedURL = arguments[3];
                                } else if (evt === 'crashed') {
                                    detail.killed = arguments[1];
                                } else if (evt === 'render-process-gone') {
                                    detail.details = arguments[1] || {};
                                } else if (evt === 'did-navigate') {
                                    detail.url = (arguments[1] || '').substring(0, 200);
                                }
                                writeDiag('webContents', 'wc#' + id + '.' + evt, detail);
                            });
                        } catch (_) {}
                    });

                    // ---- did-fail-load detail ----
                    try {
                        wc.on('did-fail-load', function (event, errorCode, errorDescription, validatedURL) {
                            var cat = 'resource_load';
                            if (errorCode < 0) cat = 'network';
                            writeDiag(cat, 'wc#' + id + '.did-fail-load', {
                                id: id, errorCode: errorCode,
                                errorDescription: errorDescription,
                                url: (validatedURL || '').substring(0, 300)
                            });
                        });
                    } catch (_) {}
                }

                writeDiag('window', 'BrowserWindow_ready', {id: id});
                return win;
            };

            // Copy static properties of BrowserWindow to PatchedBrowserWindow
            for (var key in origBW) {
                if (Object.prototype.hasOwnProperty.call(origBW, key)) {
                    PatchedBrowserWindow[key] = origBW[key];
                }
            }
            if (origBW.prototype) {
                PatchedBrowserWindow.prototype = origBW.prototype;
            }

            electron.BrowserWindow = PatchedBrowserWindow;

            writeDiag('lifecycle', 'main_diag_hooks_installed', {
                monitoredEvents: 'app events: ' + appEvents.length
            });
        }

        Module._load = function (request, parent, isMain) {
            var exports = originalLoad.apply(this, arguments);
            if (request === 'electron') {
                try {
                    applyElectronHooks(exports);
                } catch (err) {
                    writeDiag('preload', 'main_diag_hooks_application_failed', {error: err.message});
                }
            }
            return exports;
        };

        // ---- Uncaught exceptions in main process ----
        process.on('uncaughtException', function (error) {
            if (error && (error.code === 'EIO' || error.code === 'EPIPE' || (error.message && error.message.indexOf('EIO') !== -1))) {
                return;
            }
            var cat = classifyError(error);
            writeDiag(cat, 'main_uncaught_exception',
                (error.stack || error.message || String(error)).substring(0, 500));
        });

        process.on('unhandledRejection', function (reason) {
            if (reason && (reason.code === 'EIO' || reason.code === 'EPIPE' || (reason.message && reason.message.indexOf('EIO') !== -1))) {
                return;
            }
            var cat = classifyError(reason);
            writeDiag(cat, 'main_unhandled_rejection',
                (reason ? (reason.stack || reason.message || String(reason)) : '').substring(0, 500));
        });

    } catch (e) {
        writeDiag('preload', 'main_diag_module_hook_failed', {error: e.message});
    }

    writeDiag('lifecycle', 'main_diag_initialized', {});
})();
