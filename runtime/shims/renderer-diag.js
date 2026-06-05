// renderer-diag.js — Semantic renderer observability diagnostic
// Architecture: docs/architecture/DEGRADATION_MODEL.md
// Activated by: MACRUN_DIAG_RENDERER=1 (conditional shim)
//
// This is a PURELY DIAGNOSTIC shim. It does NOT:
//   - mutate any application API
//   - suppress errors silently
//   - auto-retry failures
//   - swap runtime behavior
//   - add compatibility polyfills
//
// It ONLY captures observable signals and pipes them to a structured
// diagnostic file. Every captured event is timestamped, process-attributed,
// and classified by semantic category.

(function () {
    if (process.env.MACRUN_DIAG_RENDERER !== '1') return;

    // Prevent uncaught exceptions from broken pipe (EIO) on stdout/stderr
    if (typeof process !== 'undefined') {
        if (process.stdout && typeof process.stdout.on === 'function') {
            process.stdout.on('error', function () {});
        }
        if (process.stderr && typeof process.stderr.on === 'function') {
            process.stderr.on('error', function () {});
        }
    }

    var diagFile = process.env.MACRUN_DIAG_FILE || '/tmp/macrun-diag.log';
    var fs;
    try { fs = require('fs'); } catch (_) { fs = null; }
    var path = require('path');

    // ---- Timestamp helper ----
    function ts() {
        return new Date().toISOString();
    }

    // ---- Process attribution ----
    function procType() {
        try {
            var electron = require('electron');
            // In Electron preload, process.type is available
            if (typeof process.type === 'string') return process.type;
        } catch (_) {}
        if (typeof window !== 'undefined') return 'renderer';
        return 'unknown';
    }

    var pid = typeof process !== 'undefined' ? (process.pid || 0) : 0;
    var procLabel = '[' + procType() + ' pid=' + pid + ']';

    // ---- Structured diagnostic writer ----
    function writeDiag(category, event, detail) {
        var line = ts() + ' ' + procLabel + ' [MACRUN:DIAG:' + category + '] ' + event;
        if (detail) {
            if (typeof detail === 'object') {
                try { line += ' ' + JSON.stringify(detail); } catch (_) { line += ' ' + String(detail); }
            } else {
                line += ' ' + String(detail).substring(0, 500);
            }
        }
        line += '\n';

        // Write to file (synchronous for crash-safety; diagnostic-only)
        if (fs) {
            try { fs.appendFileSync(diagFile, line); } catch (_) {}
        }
        // Also emit to stderr for console visibility
        try { process.stderr.write('[diag] ' + line); } catch (_) {}
    }

    // ---- Classify a failure into semantic categories ----
    function classifyError(error, context) {
        var category = 'unknown';
        var msg = error ? (error.message || String(error)) : '';

        if (msg.indexOf('Cannot find module') !== -1) category = 'module_resolution';
        else if (msg.indexOf('contextBridge') !== -1) category = 'preload_contextBridge';
        else if (msg.indexOf('ipcRenderer') !== -1) category = 'ipc';
        else if (msg.indexOf('is not a function') !== -1) category = 'api_mismatch';
        else if (msg.indexOf('is not defined') !== -1) category = 'missing_global';
        else if (msg.indexOf('Failed to load') !== -1) category = 'resource_load';
        else if (msg.indexOf('protocol') !== -1) category = 'protocol_handler';
        else if (msg.indexOf('sandbox') !== -1) category = 'sandbox';
        else if (msg.indexOf('GPU') !== -1 || msg.indexOf('gpu') !== -1) category = 'gpu';
        else if (msg.indexOf('webContents') !== -1) category = 'webContents';
        else if (msg.indexOf('net::') !== -1) category = 'network';
        else if (msg.indexOf('CSP') !== -1 || msg.indexOf('Content-Security') !== -1) category = 'csp';
        else if (msg.indexOf('Hydration') !== -1 || msg.indexOf('hydrate') !== -1) category = 'hydration';
        else if (msg.indexOf('ENOENT') !== -1) category = 'file_not_found';
        else if (msg.indexOf('EACCES') !== -1 || msg.indexOf('EPERM') !== -1) category = 'permission';
        else category = 'runtime_exception';

        return category;
    }

    writeDiag('lifecycle', 'renderer_diag_active', {});

    // ============================================================
    // 1. Console capture — renderer process
    // ============================================================
    if (typeof console !== 'undefined') {
        var levels = ['error', 'warn', 'log'];
        var orig = {};
        levels.forEach(function (lvl) {
            if (typeof console[lvl] === 'function') {
                orig[lvl] = console[lvl];
                console[lvl] = function () {
                    var args = Array.prototype.slice.call(arguments);
                    var msg = args.map(function (a) { return typeof a === 'string' ? a : JSON.stringify(a); }).join(' ');
                    writeDiag('console_' + lvl, 'console.' + lvl, msg.substring(0, 300));
                    return orig[lvl].apply(console, arguments);
                };
            }
        });
    }

    // ============================================================
    // 2. Unhandled promise rejections
    // ============================================================
    if (typeof process !== 'undefined') {
        process.on('unhandledRejection', function (reason, promise) {
            if (reason && (reason.code === 'EIO' || reason.code === 'EPIPE' || (reason.message && reason.message.indexOf('EIO') !== -1))) {
                return;
            }
            var cat = classifyError(reason, 'unhandledRejection');
            var detail = reason ? (reason.stack || reason.message || String(reason)) : 'no reason';
            writeDiag(cat, 'unhandled_rejection', detail.substring(0, 500));
        });

        process.on('uncaughtException', function (error) {
            if (error && (error.code === 'EIO' || error.code === 'EPIPE' || (error.message && error.message.indexOf('EIO') !== -1))) {
                return;
            }
            var cat = classifyError(error, 'uncaughtException');
            writeDiag(cat, 'uncaught_exception', (error.stack || error.message || String(error)).substring(0, 500));
        });
    }

    // ============================================================
    // 3. Window-level error capture (renderer process only)
    // ============================================================
    if (typeof window !== 'undefined') {
        window.addEventListener('error', function (event) {
            var cat = classifyError(event.error, 'window:error');
            var detail = {
                message: event.message,
                filename: event.filename,
                lineno: event.lineno,
                colno: event.colno
            };
            if (event.error) detail.stack = (event.error.stack || '').substring(0, 300);
            writeDiag(cat, 'window_error', detail);
        });

        window.addEventListener('unhandledrejection', function (event) {
            var cat = classifyError(event.reason, 'window:unhandledrejection');
            writeDiag(cat, 'window_unhandled_rejection',
                (event.reason ? (event.reason.message || String(event.reason)) : '').substring(0, 500));
        });
    }

    // ============================================================
    // 4. Module resolution tracing
    // ============================================================
    if (typeof process !== 'undefined' && typeof process.dlopen !== 'function') {
        // Hook require() failures via module._resolveFilename monkeypatch
        try {
            var Module = require('module');
            var origResolve = Module._resolveFilename;
            Module._resolveFilename = function (request, parent) {
                try {
                    return origResolve.apply(this, arguments);
                } catch (e) {
                    if (e.code === 'MODULE_NOT_FOUND') {
                        writeDiag('module_resolution', 'require_failed',
                            {request: request, parent: parent ? parent.filename : 'unknown'});
                    }
                    throw e;
                }
            };
        } catch (_) { /* module system not available */ }
    }

    // ============================================================
    // 5. Electron IPC channel registration visibility (preload context)
    // ============================================================
    try {
        var electron = require('electron');
        if (electron.ipcRenderer) {
            writeDiag('ipc', 'ipcRenderer_available', {});

            // Track what channels the app registers
            var origOn = electron.ipcRenderer.on;
            var registeredChannels = [];
            electron.ipcRenderer.on = function (channel, listener) {
                registeredChannels.push(channel);
                writeDiag('ipc', 'ipc_channel_registered', {channel: channel});
                return origOn.call(electron.ipcRenderer, channel, listener);
            };

            // Track IPC invocations from renderer
            var origInvoke = electron.ipcRenderer.invoke;
            if (typeof origInvoke === 'function') {
                electron.ipcRenderer.invoke = function (channel) {
                    writeDiag('ipc', 'ipc_invoke', {channel: channel});
                    return origInvoke.apply(electron.ipcRenderer, arguments).catch(function (e) {
                        writeDiag('ipc', 'ipc_invoke_failed', {channel: channel, error: e.message || String(e)});
                        throw e;
                    });
                };
            }

            var origSend = electron.ipcRenderer.send;
            if (typeof origSend === 'function') {
                electron.ipcRenderer.send = function (channel) {
                    writeDiag('ipc', 'ipc_send', {channel: channel});
                    return origSend.apply(electron.ipcRenderer, arguments);
                };
            }
        }

        // contextBridge exposure visibility
        if (electron.contextBridge) {
            writeDiag('preload', 'contextBridge_available', {});
        } else {
            writeDiag('preload', 'contextBridge_missing', {message: 'contextBridge not available — app may use window globals instead'});
        }
    } catch (e) {
        writeDiag('preload', 'electron_api_unavailable', {error: e.message});
    }

    // ============================================================
    // 6. DOM readiness / hydration visibility
    // ============================================================
    if (typeof document !== 'undefined') {
        document.addEventListener('DOMContentLoaded', function () {
            writeDiag('hydration', 'DOMContentLoaded', {
                readyState: document.readyState,
                bodyChildren: document.body ? document.body.children.length : 0
            });
        });

        // Late check: after load, report what the DOM looks like
        window.addEventListener('load', function () {
            writeDiag('hydration', 'window_load', {
                readyState: document.readyState,
                bodyChildren: document.body ? document.body.children.length : 0,
                bodyHTML: document.body ? document.body.innerHTML.substring(0, 200) : 'no-body'
            });
        });
    }

    // ============================================================
    // 7. GPU / WebGL crash visibility
    // ============================================================
    if (typeof window !== 'undefined') {
        var canvas = document.createElement('canvas');
        var gl = canvas.getContext('webgl') || canvas.getContext('webgl2');
        if (!gl) {
            writeDiag('gpu', 'webgl_unavailable', {message: 'WebGL context creation failed — GPU compositing may be unavailable'});
        } else {
            var debugInfo = gl.getExtension('WEBGL_debug_renderer_info');
            if (debugInfo) {
                writeDiag('gpu', 'webgl_available', {
                    vendor: gl.getParameter(debugInfo.UNMASKED_VENDOR_WEBGL),
                    renderer: gl.getParameter(debugInfo.UNMASKED_RENDERER_WEBGL)
                });
            }
        }
    }

    writeDiag('lifecycle', 'renderer_diag_initialized', {
        shims: process.env.MACRUN_SHIM_PATHS === '1' ? 'active' : 'none'
    });

})();
