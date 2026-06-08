// native-module-loader.js — Runtime preload for native module substitution
// Architecture: Native Module Compatibility Infrastructure Plan v3 § Runtime Injection
// Loaded via preload-main.js when MACRUN_SHIM_NATIVE_LOADER=1.
// Intercepts require() for known native module names, redirects to staged .node
// files. Falls back to dlopen Proxy stubs via platform-normalizer.js.
(function() {
    'use strict';

    var substitutionMap;
    try {
        substitutionMap = JSON.parse(process.env.MACRUN_NATIVE_SUBSTITUTION_MAP || '{}');
    } catch (_) {
        substitutionMap = {};
    }

    if (Object.keys(substitutionMap).length === 0) return;

    var Module = require('module');
    var originalLoad = Module._load;

    Module._load = function(request, parent, isMain) {
        if (substitutionMap.hasOwnProperty(request)) {
            var stagedPath = substitutionMap[request];
            console.warn('[macrun:native] Redirecting require("' + request + '") -> ' + stagedPath);
            try {
                return originalLoad.call(this, stagedPath, parent, isMain);
            } catch (e) {
                console.error('[macrun:native] Failed to load staged native module "' + request + '":', e.message);
                var makeProxyStub = function() {
                    var stub = function() { return makeProxyStub(); };
                    return new Proxy(stub, {
                        get: function(_, prop) {
                            if (prop === 'then') return undefined;
                            if (prop === 'toString') return function() { return '[object MacRunNativeStub]'; };
                            return makeProxyStub();
                        },
                        apply: function() { return makeProxyStub(); }
                    });
                };
                return makeProxyStub();
            }
        }
        return originalLoad.apply(this, arguments);
    };
})();
