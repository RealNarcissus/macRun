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

var path = require('path');
var asarPath = process.argv[2];
var destPath = process.argv[3];

if (!asarPath || !destPath) {
    console.error('Usage: node asar-extract.js <asar_file> <dest_dir>');
    process.exit(1);
}

try {
    var asar = require('@electron/asar');
    asar.extractAll(asarPath, destPath);
    process.exit(0);
} catch (e) {
    if (e.code === 'MODULE_NOT_FOUND') {
        console.error('macrun: @electron/asar not installed. Run acquire.sh first.');
    } else {
        console.error('macrun: ASAR extraction failed: ' + e.message);
    }
    process.exit(2);
}
