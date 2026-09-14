import test from 'node:test';
import assert from 'node:assert/strict';
import { parseSessionPersistenceState } from '../dist/ipc/session-persistence-protocol.js';

test('accepts saved and active modes independently for restart notice', () => {
  assert.deepEqual(parseSessionPersistenceState('{"version":1,"enabled":false,"active":false}'),
    {version: 1, enabled: false, active: false});
  assert.deepEqual(parseSessionPersistenceState('{"version":1,"enabled":true,"active":false}'),
    {version: 1, enabled: true, active: false});
});

test('rejects malformed, untrusted and oversized preference responses', () => {
  for (const raw of ['{', 'null', '[]', '{}',
    '{"version":1,"enabled":"true","active":false}',
    '{"version":1,"enabled":false,"active":0}',
    '{"version":2,"enabled":false,"active":false}',
    '{"version":1,"enabled":false,"active":false,"extra":1}',
    ' '.repeat(257)]) assert.throws(() => parseSessionPersistenceState(raw));
});
