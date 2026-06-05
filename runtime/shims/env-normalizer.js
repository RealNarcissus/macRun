// env-normalizer.js — always-active environment normalization shim
// Normalizes process.env to Linux equivalents. Sets MACRUN_ORIGINAL_PLATFORM
// so apps can detect they're running under runtime substitution if needed.
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0

(function normalize_environment() {
    const os = require('os');

    // Record original values before any normalization
    process.env.MACRUN_ORIGINAL_PLATFORM = process.platform;

    // Normalize HOME — macOS .asar may reference /Users/<name>
    if (!process.env.MACRUN_HOME_ORIGINAL && process.env.HOME) {
        process.env.MACRUN_HOME_ORIGINAL = process.env.HOME;
    }

    // Normalize USER
    if (!process.env.USER && process.env.HOME) {
        try {
            process.env.USER = os.userInfo().username;
        } catch (_) {
            process.env.USER = 'unknown';
        }
    }

    // Normalize TMPDIR
    if (!process.env.TMPDIR || process.env.TMPDIR.startsWith('/var/folders')) {
        process.env.TMPDIR = process.env.TMPDIR || '/tmp';
    }

    // Normalize SHELL
    if (!process.env.SHELL) {
        process.env.SHELL = '/bin/bash';
    }

    // Normalize LANG
    if (!process.env.LANG) {
        process.env.LANG = 'en_US.UTF-8';
    }

    // Ensure standard Linux environment variables
    if (!process.env.XDG_CACHE_HOME && process.env.HOME) {
        process.env.XDG_CACHE_HOME = process.env.HOME + '/.cache';
    }
    if (!process.env.XDG_CONFIG_HOME && process.env.HOME) {
        process.env.XDG_CONFIG_HOME = process.env.HOME + '/.config';
    }
    if (!process.env.XDG_DATA_HOME && process.env.HOME) {
        process.env.XDG_DATA_HOME = process.env.HOME + '/.local/share';
    }
})();
