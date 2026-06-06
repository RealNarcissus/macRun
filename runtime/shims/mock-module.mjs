import { createRequire as nodeCreateRequire } from 'module';
const require = nodeCreateRequire(import.meta.url);
const realModule = require('node:module');

export const Module = realModule.Module;
export const builtinModules = realModule.builtinModules;
export const isBuiltin = realModule.isBuiltin;
export const syncBuiltinESMExports = realModule.syncBuiltinESMExports;
export const findSourceMap = realModule.findSourceMap;
export const SourceMap = realModule.SourceMap;

export const createRequire = realModule.createRequire;

export const register = realModule.register || function(specifier, parentURL) {
  console.warn('[macrun:mock-module] register called (no-op) for:', specifier);
};

export default {
  Module,
  builtinModules,
  isBuiltin,
  syncBuiltinESMExports,
  findSourceMap,
  SourceMap,
  createRequire,
  register
};
