import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const realDC = require('node:diagnostics_channel');

export const channel = realDC.channel;
export const hasSubscribers = realDC.hasSubscribers;
export const subscribe = realDC.subscribe;
export const unsubscribe = realDC.unsubscribe;
export const Channel = realDC.Channel;

export const tracingChannel = realDC.tracingChannel || function(name) {
  return {
    name,
    subscribe: () => {},
    unsubscribe: () => {},
    start: () => {},
    end: { subscribe: () => {}, unsubscribe: () => {} },
    asyncStart: () => {},
    asyncEnd: () => {},
    error: () => {},
    hasSubscribers: false
  };
};

export default {
  channel,
  hasSubscribers,
  subscribe,
  unsubscribe,
  Channel,
  tracingChannel
};
