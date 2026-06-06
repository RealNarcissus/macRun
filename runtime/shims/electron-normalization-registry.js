// electron-normalization-registry.js — Governed Electron API Normalization
// Architecture: docs/architecture/ELECTRON_API_NORMALIZATION.md
//   docs/architecture/SHIM_GOVERNANCE.md
// Activated by: MACRUN_SHIM_NORMALIZATION=1 (conditional shim)
//
// This is the SINGLE authoritative normalization registry. Every API patch must
// be registered here with: version scope, risk class, degradation category,
// and a deterministic apply() function. NO ad-hoc patches outside this file.
//
// This shim is PURELY a normalization delivery mechanism. It does NOT:
//   - Decide which APIs to patch (the registry decides)
//   - Mutate behavior outside registered entries
//   - Apply normalizations silently
//   - Make app-specific decisions

(function () {
    if (process.env.MACRUN_SHIM_NORMALIZATION !== '1') return;

    var electron;
    try { electron = require('electron'); } catch (_) { electron = null; }
    if (!electron) return;

    // ---- Electron version detection ----
    var version = 'unknown';
    try {
        version = process.versions.electron || version;
    } catch (_) {}

    // ---- Diagnostic writer ----
    function emitNormalization(id, riskClass, apiTarget, detail) {
        var line = '[MACRUN:NORMALIZATION] class=' + riskClass +
                   ' api=' + apiTarget +
                   ' version=' + version +
                   ' id=' + id +
                   ' ' + (detail || '');
        console.warn(line);
        // Also emit to stderr for adapter log capture
        try { process.stderr.write(line + '\n'); } catch (_) {}
    }

    // ---- Version comparison helper ----
    function versionInRange(current, min, max) {
        if (!current || current === 'unknown') return false;
        var cur = current.split('.').map(Number);
        var lo  = min.split('.').map(Number);
        var hi  = max.split('.').map(Number);
        for (var i = 0; i < 3; i++) {
            var c = cur[i] || 0;
            var l = lo[i] || 0;
            var h = hi[i] || 999;
            if (c < l) return false;
            if (c > h) return false;
            if (c > l) return true;   // definitively above minimum
            if (c < h) return true;   // definitively below maximum
        }
        return true; // exact match or all equal
    }

    // ============================================================
    // NORMALIZATION REGISTRY
    // Each entry: { id, target, min, max, riskClass, degradation, apply }
    // ============================================================

    var REGISTRY = [

        // --- Class A: setBackgroundColor no-op ---
        // Electron 28 Linux does not expose WebContentsView.prototype.setBackgroundColor.
        // This is a purely cosmetic API with no rendering impact on Linux.
        {
            id: 'setBackgroundColor-noop',
            target: 'WebContentsView.prototype.setBackgroundColor',
            min: '28.0.0',
            max: '28.99.99',
            riskClass: 'A',
            degradation: 'transparent',
            reason: 'setBackgroundColor is not exposed in Electron 28 Linux. No-op stub.',
            apply: function (electron) {
                if (!electron.WebContentsView) return false;
                var proto = electron.WebContentsView.prototype;
                if (typeof proto.setBackgroundColor === 'function') return false; // already exists

                proto.setBackgroundColor = function () {
                    // Cosmetic no-op. setBackgroundColor sets the default background
                    // for new WebContentsViews. On Linux, the compositor handles
                    // the initial background. Returning without effect is safe.
                };
                return true;
            }
        },

        // --- Class A: setVisible no-op ---
        // Electron 28 Linux does not expose WebContentsView.prototype.setVisible.
        // On Linux, visibility is managed via parent contentView addChildView/removeChildView.
        // Thus, setVisible is a macOS-only API that can be safely stubbed as a no-op on Linux.
        {
            id: 'setVisible-noop',
            target: 'WebContentsView.prototype.setVisible',
            min: '28.0.0',
            max: '28.99.99',
            riskClass: 'A',
            degradation: 'transparent',
            reason: 'setVisible is not exposed in Electron 28 Linux. No-op stub.',
            apply: function (electron) {
                if (!electron.WebContentsView) return false;
                var proto = electron.WebContentsView.prototype;
                if (typeof proto.setVisible === 'function') return false; // already exists

                proto.setVisible = function (visible) {
                    // Cosmetic/structural no-op. Visibility on Linux is managed by adding/removing from parent view.
                };
                return true;
            }
        },

        // --- Class B: WebContentsView and contentView bridge ---
        // Maps modern Electron 30+ WebContentsView and win.contentView APIs to Electron 28 BrowserView APIs.
        {
            id: 'contentView-bridge',
            target: 'BrowserWindow.prototype.contentView',
            min: '28.0.0',
            max: '28.99.99',
            riskClass: 'B',
            degradation: 'shimmed',
            reason: 'Map WebContentsView and contentView to BrowserView APIs in Electron 28 Linux.',
            apply: function (electron) {
                if (!electron.BrowserView) return false;

                // 1. Alias WebContentsView to BrowserView
                var originalBrowserView = electron.BrowserView;
                electron.WebContentsView = originalBrowserView;

                // Make sure instanceof WebContentsView works for BrowserView instances
                try {
                    Object.defineProperty(originalBrowserView, Symbol.hasInstance, {
                        value: function (instance) {
                            return instance && instance.constructor === originalBrowserView;
                        },
                        configurable: true
                    });
                } catch (_) {}

                // 2. Define custom setVisible and setBounds on BrowserView prototype for visibility control
                var originalSetBounds = originalBrowserView.prototype.setBounds;

                originalBrowserView.prototype.setVisible = function (visible) {
                    this._visible = visible;
                    if (visible) {
                        if (this._lastBounds) {
                            originalSetBounds.call(this, this._lastBounds);
                        }
                    } else {
                        var bounds = this.getBounds();
                        if (bounds.width !== 0 || bounds.height !== 0) {
                            this._lastBounds = bounds;
                        }
                        originalSetBounds.call(this, { x: 0, y: 0, width: 0, height: 0 });
                    }
                };

                originalBrowserView.prototype.setBounds = function (bounds) {
                    this._lastBounds = bounds;
                    if (this._visible !== false) {
                        originalSetBounds.call(this, bounds);
                    }
                };

                // 3. Define getter for contentView on BrowserWindow.prototype and BaseWindow.prototype
                function defineContentView(proto) {
                    if (!proto) return;
                    Object.defineProperty(proto, 'contentView', {
                        get: function () {
                            if (!this._mockContentView) {
                                const self = this;
                                this._mockContentView = {
                                    addChildView: function (view) {
                                        self.addBrowserView(view);
                                    },
                                    removeChildView: function (view) {
                                        self.removeBrowserView(view);
                                    }
                                };
                            }
                            return this._mockContentView;
                        },
                        configurable: true
                    });
                }

                defineContentView(electron.BrowserWindow.prototype);
                if (electron.BaseWindow) {
                    defineContentView(electron.BaseWindow.prototype);
                }

                return true;
            }
        },

        // --- Class B: webContents.navigationHistory bridge ---
        // Maps modern Electron 30+ webContents.navigationHistory APIs to direct webContents methods in Electron 28.
        {
            id: 'navigationHistory-bridge',
            target: 'webContents.navigationHistory',
            min: '28.0.0',
            max: '28.99.99',
            riskClass: 'B',
            degradation: 'shimmed',
            reason: 'Map webContents.navigationHistory to direct methods in Electron 28.',
            apply: function (electron) {
                var prototypePatched = false;
                function ensureNavigationHistory(wc) {
                    if (prototypePatched) return;
                    try {
                        var proto = Object.getPrototypeOf(wc);
                        if (proto && !proto.navigationHistory) {
                            Object.defineProperty(proto, 'navigationHistory', {
                                get: function () {
                                    var self = this;
                                    return {
                                        canGoBack: function () { return self.canGoBack(); },
                                        canGoForward: function () { return self.canGoForward(); },
                                        goBack: function () { return self.goBack(); },
                                        goForward: function () { return self.goForward(); },
                                        clearHistory: function () { return self.clearHistory ? self.clearHistory() : undefined; }
                                    };
                                },
                                configurable: true
                            });
                            prototypePatched = true;
                        }
                    } catch (_) {}
                }

                // Intercept BrowserWindow.prototype.webContents getter
                if (electron.BrowserWindow && electron.BrowserWindow.prototype) {
                    var desc = Object.getOwnPropertyDescriptor(electron.BrowserWindow.prototype, 'webContents');
                    if (desc && desc.get) {
                        var originalGet = desc.get;
                        Object.defineProperty(electron.BrowserWindow.prototype, 'webContents', {
                            get: function () {
                                var wc = originalGet.call(this);
                                if (wc) {
                                    ensureNavigationHistory(wc);
                                }
                                return wc;
                            },
                            configurable: true
                        });
                    }
                }

                // Intercept BrowserView.prototype.webContents / WebContentsView.prototype.webContents getter
                var viewClass = electron.WebContentsView || electron.BrowserView;
                if (viewClass && viewClass.prototype) {
                    var descView = Object.getOwnPropertyDescriptor(viewClass.prototype, 'webContents');
                    if (descView && descView.get) {
                        var originalGetView = descView.get;
                        Object.defineProperty(viewClass.prototype, 'webContents', {
                            get: function () {
                                var wc = originalGetView.call(this);
                                if (wc) {
                                    ensureNavigationHistory(wc);
                                }
                                return wc;
                            },
                            configurable: true
                        });
                    }
                }

                // Intercept app web-contents-created event to ensure any listener gets it
                if (electron.app) {
                    var originalOn = electron.app.on;
                    if (typeof originalOn === 'function') {
                        electron.app.on = function (event, listener) {
                            if (event === 'web-contents-created') {
                                var wrappedListener = function (e, wc) {
                                    if (wc) {
                                        ensureNavigationHistory(wc);
                                    }
                                    return listener.apply(this, arguments);
                                };
                                listener.__wrapped = wrappedListener;
                                return originalOn.call(this, event, wrappedListener);
                            }
                            return originalOn.apply(this, arguments);
                        };
                    }
                    var originalAddListener = electron.app.addListener;
                    if (typeof originalAddListener === 'function') {
                        electron.app.addListener = function (event, listener) {
                            if (event === 'web-contents-created') {
                                var wrappedListener = function (e, wc) {
                                    if (wc) {
                                        ensureNavigationHistory(wc);
                                    }
                                    return listener.apply(this, arguments);
                                };
                                listener.__wrapped = wrappedListener;
                                return originalAddListener.call(this, event, wrappedListener);
                            }
                            return originalAddListener.apply(this, arguments);
                        };
                    }
                }

                return true;
            }
        },

        // --- Class B: process.platform targeted bypass ---
        // Bypasses macOS platform support checks in macOS Electron apps (like Claude Desktop)
        // by returning 'darwin' ONLY for specific platform-validation call stacks.
        {
            id: 'platform-check-bypass',
            target: 'process.platform',
            min: '0.0.0',
            max: '999.99.99',
            riskClass: 'B',
            degradation: 'shimmed',
            reason: 'Bypass macOS platform validation checks in application main bundle.',
            apply: function (electron) {
                var originalPlatform = process.platform;
                try {
                    Object.defineProperty(process, 'platform', {
                        get: function () {
                            var stack = '';
                            try {
                                stack = new Error().stack || '';
                            } catch (_) {}

                            var appDir = process.env.MACRUN_EXTRACTED_APP_DIR;
                            var isAppSource = appDir && stack.indexOf(appDir) !== -1;

                            // Targeted method/class validation check frames
                            var hasPlatformCheck = stack.indexOf('getHostPlatform') !== -1 ||
                                                   stack.indexOf('ZuA') !== -1 ||
                                                   stack.indexOf('xIr') !== -1 ||
                                                   stack.indexOf('createDarwinExecutor') !== -1;

                            if (isAppSource && hasPlatformCheck) {
                                return 'darwin';
                            }
                            return originalPlatform;
                        },
                        configurable: true
                    });
                    return true;
                } catch (e) {
                    console.error('[macrun-shim] Failed to define process.platform getter:', e.message);
                    return false;
                }
            }
        }
    ];

    // ============================================================
    // Apply all matching normalizations
    // ============================================================

    var applied = 0;
    var skipped = 0;
    var degraded = false;

    for (var i = 0; i < REGISTRY.length; i++) {
        var entry = REGISTRY[i];

        // Version scope check
        if (!versionInRange(version, entry.min, entry.max)) {
            skipped++;
            continue;
        }

        // Apply the normalization
        try {
            var wasApplied = entry.apply(electron);
            if (wasApplied) {
                applied++;
                emitNormalization(entry.id, entry.riskClass, entry.target,
                    'degradation=' + entry.degradation + ' reason=' + entry.reason);

                // Degradation tracking via stderr for adapter capture
                if (entry.riskClass === 'B' || entry.riskClass === 'C') {
                    degraded = true;
                }
            } else {
                // API already present or normalization not applicable
                skipped++;
            }
        } catch (e) {
            console.error('[macrun-shim] Normalization ' + entry.id + ' failed: ' + e.message);
        }
    }

    // Summary
    var summary = applied + ' normalization(s) applied, ' + skipped + ' skipped' +
                  ' for Electron ' + version;
    if (degraded) summary += ' (DEGRADED)';
    try { process.stderr.write('[MACRUN:NORMALIZATION] ' + summary + '\n'); } catch (_) {}

})();
