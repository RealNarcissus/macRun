// platform-normalizer.js — overrides Electron platform detection
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
//
// Some macOS Electron apps use process.platform === 'darwin' to gate features.
// This shim normalizes platform to 'linux' and provides compatibility stubs
// for common macOS-specific Electron APIs that apps check at startup.

(function normalize_platform() {
    // globalThis.crypto fallback for Node 18 environments
    if (typeof globalThis !== 'undefined' && typeof globalThis.crypto === 'undefined') {
        try {
            Object.defineProperty(globalThis, 'crypto', {
                value: require('node:crypto').webcrypto,
                configurable: true,
                writable: true
            });
        } catch (_) {}
    }

    // Redefine process.resourcesPath to point to the extracted app directory
    if (typeof process !== 'undefined' && process.env.MACRUN_EXTRACTED_APP_DIR) {
        try {
            Object.defineProperty(process, 'resourcesPath', {
                get: function () { return process.env.MACRUN_EXTRACTED_APP_DIR; },
                configurable: true
            });
        } catch (_) {}
    }

    // process.platform is read-only; we can't override it directly.
    // Instead, we stub macOS-specific Electron APIs that apps commonly check.
    // The Electron binary running natively on Linux already reports 'linux' for
    // process.platform. This shim handles the case where the app's .asar was
    // built for macOS and contains conditional darwin-only code paths.

    try {
        const Module = require('module');
        const originalLoad = Module._load;
        const mockInspectorPromises = {
            Session: class Session {
                connect() { return Promise.resolve(); }
                disconnect() { return Promise.resolve(); }
                post() { return Promise.resolve(); }
                on() {}
                once() {}
                addListener() {}
                removeListener() {}
            }
        };
        Module._load = function (request, parent, isMain) {
            if (request === 'node:inspector/promises' || request === 'inspector/promises') {
                console.warn('[macrun-shim] Stubbed missing module:', request);
                return mockInspectorPromises;
            }
            return originalLoad.apply(this, arguments);
        };
    } catch (e) {
        console.error('[macrun-shim] Failed to patch Module._load:', e.message);
    }

    try {
        const electron = require('electron');

        // systemPreferences — commonly checked by Electron apps for dark mode,
        // accessibility, and media keys. Provide Linux-safe defaults.
        if (electron.systemPreferences) {
            const orig = {};
            for (const key of ['isDarkMode', 'getUserDefault', 'getColor',
                               'isTrustedAccessibilityClient', 'subscribeNotification',
                               'unsubscribeNotification', 'getMediaAccessStatus']) {
                if (typeof electron.systemPreferences[key] === 'function') {
                    orig[key] = electron.systemPreferences[key];
                }
            }

            if (!electron.systemPreferences._macrun_patched) {
                electron.systemPreferences.isDarkMode = function () { return false; };
                electron.systemPreferences.getUserDefault = function (key, type) {
                    return type === 'boolean' ? false : type === 'string' ? '' : null;
                };
                electron.systemPreferences.isTrustedAccessibilityClient = function (prompt) {
                    return false;
                };
                electron.systemPreferences.getMediaAccessStatus = function (mediaType) {
                    return 'not-determined';
                };
                electron.systemPreferences._macrun_patched = true;
            }
        }

        // app.isPackaged — macOS apps sometimes check this; always true for .asar
        if (electron.app) {
            try {
                Object.defineProperty(electron.app, 'isPackaged', {
                    get: function () { return true; },
                    configurable: true
                });
            } catch (_) { /* property already defined */ }
        }

        // nativeTheme — safely stub if the Electron version supports it
        if (electron.nativeTheme) {
            try {
                if (!electron.nativeTheme._macrun_patched) {
                    const origShouldUseDarkColors = electron.nativeTheme.shouldUseDarkColors;
                    const origShouldUseHighContrastColors = electron.nativeTheme.shouldUseHighContrastColors;
                    Object.defineProperty(electron.nativeTheme, 'shouldUseDarkColors', {
                        get: function () { return false; },
                        configurable: true
                    });
                    Object.defineProperty(electron.nativeTheme, 'shouldUseHighContrastColors', {
                        get: function () { return false; },
                        configurable: true
                    });
                    electron.nativeTheme._macrun_patched = true;
                }
            } catch (_) { /* nativeTheme not available */ }
        }

    } catch (_) {
        // Electron APIs not available (non-Electron Node.js context) — safe to ignore
    }

    // Stub out require('os').platform() via module caching
    // This is fragile; we rely on the app using process.platform which is
    // already 'linux' when running under Linux Electron.

    // Robust safety net: catch loading failures of Darwin-native modules and stub them with a recursive Proxy.
    if (typeof process.dlopen === 'function') {
        var origDlopen = process.dlopen;
        process.dlopen = function (module, filename, flags) {
            try {
                return origDlopen.apply(this, arguments);
            } catch (e) {
                console.warn('[macrun-shim] Failed to load native module:', filename, '; Stubbing with a recursive Proxy to prevent crash. Error:', e.message);
                
                // Construct a recursive Proxy that acts as a function and returns itself for any property access
                var makeProxyStub = function () {
                    var stub = function () { return makeProxyStub(); };
                    return new Proxy(stub, {
                        get: function (target, prop) {
                            if (typeof prop === 'symbol') {
                                if (prop === Symbol.toStringTag) return 'MacRunStub';
                                if (prop === Symbol.toPrimitive) return function(hint) {
                                    if (hint === 'string') return '[object MacRunStub]';
                                    return 0;
                                };
                                return undefined;
                            }
                            if (prop === 'then') return undefined; // promise detection safety
                            if (prop === 'inspect' || prop === 'prototype') return undefined;
                            if (prop === 'toString') return function() { return '[object MacRunStub]'; };
                            if (prop === 'valueOf') return function() { return 0; };
                            return makeProxyStub();
                        },
                        apply: function (target, thisArg, argumentsList) {
                            return makeProxyStub();
                        }
                    });
                };

                module.exports = makeProxyStub();
                return; // Conform to Node's dlopen contract: return undefined (void).
            }
        };
    }
})();
