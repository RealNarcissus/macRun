#!/usr/bin/env node
// asar-extract.js — Deterministic ASAR extraction helper
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
//
// Invoked by the ElectronAdapter via fork+execvp with argument array.
// No shell string construction — input paths are positional argv arguments.
//
// The 'asar' npm package must be installed locally before this script runs.
// acquire.sh handles this during Electron runtime acquisition.
//
// Usage: node asar-extract.js <asar_file> <dest_dir>
//   arg[2] = path to .asar archive
//   arg[3] = path to extraction destination directory

import fs from 'node:fs';
import path from 'node:path';

// Helper to check if a path is part of an unpacked directory
function isUnpackedPath(p) {
    if (!p) return false;
    const str = typeof p === 'string' ? p : p.toString();
    return str.includes('.unpacked') || str.includes('app.asar.unpacked');
}

// 1. Mock sync APIs
const originalReadFileSync = fs.readFileSync;
fs.readFileSync = function (p, options) {
    try {
        return originalReadFileSync(p, options);
    } catch (e) {
        if (e.code === 'ENOENT' && isUnpackedPath(p)) {
            console.warn(`[macrun:asar] Warning: Mocking missing sync file: ${p}`);
            return Buffer.alloc(0);
        }
        throw e;
    }
};

const originalStatSync = fs.statSync;
fs.statSync = function (p, options) {
    try {
        return originalStatSync(p, options);
    } catch (e) {
        if (e.code === 'ENOENT' && isUnpackedPath(p)) {
            console.warn(`[macrun:asar] Warning: Mocking missing sync stat: ${p}`);
            return {
                mode: 0o644,
                size: 0,
                isFile: () => true,
                isDirectory: () => false,
                isSymbolicLink: () => false
            };
        }
        throw e;
    }
};

// 2. Mock async callback APIs
const originalReadFile = fs.readFile;
fs.readFile = function (p, options, callback) {
    if (typeof options === 'function') {
        callback = options;
        options = undefined;
    }
    originalReadFile(p, options, (err, data) => {
        if (err && err.code === 'ENOENT' && isUnpackedPath(p)) {
            console.warn(`[macrun:asar] Warning: Mocking missing async file: ${p}`);
            return callback(null, Buffer.alloc(0));
        }
        callback(err, data);
    });
};

const originalStat = fs.stat;
fs.stat = function (p, options, callback) {
    if (typeof options === 'function') {
        callback = options;
        options = undefined;
    }
    originalStat(p, options, (err, stats) => {
        if (err && err.code === 'ENOENT' && isUnpackedPath(p)) {
            console.warn(`[macrun:asar] Warning: Mocking missing async stat: ${p}`);
            return callback(null, {
                mode: 0o644,
                size: 0,
                isFile: () => true,
                isDirectory: () => false,
                isSymbolicLink: () => false
            });
        }
        callback(err, stats);
    });
};

// 3. Mock promises APIs
if (fs.promises) {
    const originalPromisesReadFile = fs.promises.readFile;
    fs.promises.readFile = async function (p, options) {
        try {
            return await originalPromisesReadFile(p, options);
        } catch (e) {
            if (e.code === 'ENOENT' && isUnpackedPath(p)) {
                console.warn(`[macrun:asar] Warning: Mocking missing promise file: ${p}`);
                return Buffer.alloc(0);
            }
            throw e;
        }
    };

    const originalPromisesStat = fs.promises.stat;
    fs.promises.stat = async function (p, options) {
        try {
            return await originalPromisesStat(p, options);
        } catch (e) {
            if (e.code === 'ENOENT' && isUnpackedPath(p)) {
                console.warn(`[macrun:asar] Warning: Mocking missing promise stat: ${p}`);
                return {
                    mode: 0o644,
                    size: 0,
                    isFile: () => true,
                    isDirectory: () => false,
                    isSymbolicLink: () => false
                };
            }
            throw e;
        }
    };
}

// Now dynamically import asar
const asarPath = process.argv[2];
const destPath = process.argv[3];

if (!asarPath || !destPath) {
    console.error('Usage: node asar-extract.js <asar_file> <dest_dir>');
    process.exit(1);
}

try {
    const asar = await import('@electron/asar');
    asar.extractAll(asarPath, destPath);
    process.exit(0);
} catch (e) {
    if (e.code === 'ERR_MODULE_NOT_FOUND' || e.code === 'MODULE_NOT_FOUND') {
        console.error('macrun: @electron/asar not installed. Run acquire.sh first.');
    } else {
        console.error('macrun: ASAR extraction failed: ' + e.message);
    }
    process.exit(2);
}
