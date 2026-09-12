import test from 'node:test';
import assert from 'node:assert/strict';
import { NativeClient } from '../dist/ipc/client.js';

const state = JSON.stringify({ version: 1, tabs: [], activeTab: 0 });
const request = { version: 1, command: 'state' };
const tick = () => new Promise(resolve => setImmediate(resolve));

test('bridge rejects standalone presentation without inventing browser state', async () => {
  globalThis.window = {};
  await assert.rejects(new NativeClient().request(request), /Native connection unavailable/);
});

test('bridge serializes a mutation behind polling and recovers after native failure', async () => {
  const calls = [];
  globalThis.window = { setTimeout, clearTimeout, cefQuery: query => { calls.push(query); return calls.length; } };
  const client = new NativeClient();
  const first = client.request(request);
  const second = client.request({ version: 1, command: 'createTab' });
  await tick();
  assert.equal(calls.length, 1);
  const rejected = assert.rejects(first, /Native failure/);
  calls[0].onFailure(400, 'Native failure');
  await rejected;
  await tick();
  assert.equal(calls.length, 2);
  assert.equal(JSON.parse(calls[1].request).command, 'createTab');
  calls[1].onSuccess(state);
  assert.equal((await second).version, 1);
});

test('bridge cancels timed-out queries and rejects invalid native snapshots', async () => {
  let expire;
  let query;
  let canceled;
  globalThis.window = {
    setTimeout: callback => { expire = callback; return 99; }, clearTimeout: () => {},
    cefQuery: value => { query = value; return 42; }, cefQueryCancel: id => { canceled = id; },
  };
  const client = new NativeClient();
  const timedOut = client.request(request);
  const rejected = assert.rejects(timedOut, /not responding/);
  await tick(); expire(); await rejected;
  assert.equal(canceled, 42);
  const invalid = client.request(request);
  const invalidRejected = assert.rejects(invalid, /Unsupported native state/);
  await tick(); query.onSuccess('{}'); await invalidRejected;
});

test('bridge clears deadline when native query throws synchronously', async () => {
  let cleared = false;
  globalThis.window = {
    setTimeout: () => 99, clearTimeout: id => { cleared = id === 99; },
    cefQuery: () => { throw new Error('Renderer shutting down'); },
  };
  await assert.rejects(new NativeClient().request(request), /Renderer shutting down/);
  assert.equal(cleared, true);
});
