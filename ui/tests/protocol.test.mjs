import test from 'node:test';
import assert from 'node:assert/strict';
import { parseState } from '../dist/ipc/protocol.js';

const tab = { id: 1, url: 'https://example.com', title: '<script>untrusted</script>', active: true, muted: false, volume: 100 };
const state = { version: 1, tabs: [tab], activeTab: 1, bookmarks: [] };
test('accepts a native snapshot while preserving page titles as data', () => {
  assert.deepEqual(parseState(JSON.stringify(state)), state);
});
test('rejects malformed or incompatible IPC', () => {
  for (const value of [null, {}, { ...state, version: 2 }, { ...state, activeTab: '1' },
    { ...state, bookmarks: undefined }, { ...state, bookmarks: 'no' },
    { ...state, tabs: [{ ...tab, id: -1 }] }, { ...state, tabs: [{ ...tab, title: 7 }] },
    { ...state, tabs: [tab, tab] }, { ...state, activeTab: 2 }, { ...state, tabs: [{ ...tab, active: false }] }]) {
    assert.throws(() => parseState(JSON.stringify(value)));
  }
  assert.throws(() => parseState('{'));
});
test('accepts empty startup state', () => {
  assert.deepEqual(parseState('{"version":1,"tabs":[],"activeTab":0,"bookmarks":[]}'), { version: 1, tabs: [], activeTab: 0, bookmarks: [] });
});

test('validates native per-tab audio values without coupling tabs', () => {
  const snapshot = { ...state, tabs: [{ ...tab, volume: 37, muted: true }, { ...tab, id: 2, active: false }] };
  assert.deepEqual(parseState(JSON.stringify(snapshot)), snapshot);
  for (const volume of [-1,101,0.5,'50',null,undefined]) {
    assert.throws(() => parseState(JSON.stringify({ ...state, tabs:[{...tab,volume}] })));
  }
  for (const muted of [0,1,'false',null,undefined]) {
    assert.throws(() => parseState(JSON.stringify({ ...state, tabs:[{...tab,muted}] })));
  }
});

test('validates bookmark entries and rejects malformed bookmark data', () => {
  const bookmarks = [
    { id: 1, title: 'Example', url: 'https://example.com' },
    { id: 2, title: 'Untitled', url: 'https://example.org/a' },
  ];
  const snapshot = { ...state, bookmarks };
  assert.deepEqual(parseState(JSON.stringify(snapshot)), snapshot);
  for (const bookmarks of [[{ id: 0, title: 'x', url: 'https://e.com' }], [{ id: 1, title: 'x', url: '' }],
    [{ id: 1, title: 7, url: 'https://e.com' }], [{ id: 1, title: 'x', url: 7 }],
    [{ id: 1, title: 'x', url: 'https://e.com' }, { id: 1, title: 'y', url: 'https://o.org' }],
    [{ id: 1.5, title: 'x', url: 'https://e.com' }]]) {
    assert.throws(() => parseState(JSON.stringify({ ...state, bookmarks })), /Invalid native bookmark/);
  }
});

test('accepts optional native maximize state and rejects malformed window data', () => {
  for (const maximized of [true, false]) {
    const snapshot = { ...state, window: { maximized } };
    assert.deepEqual(parseState(JSON.stringify(snapshot)), snapshot);
  }
  for (const window of [null, [], {}, true, { maximized: 1 }, { maximized: 'false' }]) {
    assert.throws(() => parseState(JSON.stringify({ ...state, window })), /Invalid native window state/);
  }
});