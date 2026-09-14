export interface SessionPersistenceState {
  version: 1;
  enabled: boolean;
  active: boolean;
}

export function parseSessionPersistenceState(raw: string): SessionPersistenceState {
  if (raw.length > 256) throw new Error('Invalid session preference response');
  const value: unknown = JSON.parse(raw);
  if (typeof value !== 'object' || value === null || Array.isArray(value))
    throw new Error('Invalid session preference response');
  const state = value as Record<string, unknown>;
  if (Object.keys(state).length !== 3 || state.version !== 1 ||
      typeof state.enabled !== 'boolean' || typeof state.active !== 'boolean')
    throw new Error('Invalid session preference response');
  return state as unknown as SessionPersistenceState;
}
