export async function resolve(specifier, context, nextResolve) {
  if ((specifier === 'node:diagnostics_channel' || specifier === 'diagnostics_channel') &&
      (!context.parentURL || !context.parentURL.includes('mock-diagnostics-channel'))) {
    return {
      shortCircuit: true,
      url: new URL('./mock-diagnostics-channel.mjs', import.meta.url).href
    };
  }
  if ((specifier === 'node:module' || specifier === 'module') &&
      (!context.parentURL || !context.parentURL.includes('mock-module'))) {
    return {
      shortCircuit: true,
      url: new URL('./mock-module.mjs', import.meta.url).href
    };
  }
  return nextResolve(specifier, context);
}
