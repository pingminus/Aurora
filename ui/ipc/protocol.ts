export type Command = 'state' | 'createTab' | 'closeTab' | 'activateTab' | 'navigate' |
  'back' | 'forward' | 'reload' | 'duplicateTab' | 'reopenTab' | 'devtools' |
  'minimizeWindow' | 'toggleMaximize' | 'closeWindow' | 'setTabMuted' | 'setTabVolume';
export interface Request { version: 1; command: Command; tabId?: number; url?: string; muted?: boolean; volume?: number }
export interface Tab { id: number; url: string; title: string; active: boolean; muted: boolean; volume: number; audioError?: string }
export interface BrowserState { version: 1; tabs: Tab[]; activeTab: number; window?: { maximized: boolean } }

export function parseState(raw: string): BrowserState {
  const data: unknown = JSON.parse(raw);
  if (!data || typeof data !== 'object') throw new Error('Invalid native state');
  const value = data as Record<string, unknown>;
  if (value.version !== 1 || !Array.isArray(value.tabs) || !Number.isSafeInteger(value.activeTab)) {
    throw new Error('Unsupported native state');
  }
  if ('window' in value && (!value.window || typeof value.window !== 'object' ||
    Array.isArray(value.window) || typeof (value.window as Record<string, unknown>).maximized !== 'boolean')) {
    throw new Error('Invalid native window state');
  }
  const seen = new Set<number>();
  for (const tab of value.tabs) {
    if (!tab || !Number.isSafeInteger(tab.id) || tab.id <= 0 || seen.has(tab.id) ||
      typeof tab.url !== 'string' || typeof tab.title !== 'string' || typeof tab.active !== 'boolean' ||
      typeof tab.muted !== 'boolean' || !Number.isInteger(tab.volume) || tab.volume < 0 || tab.volume > 100 ||
      (tab.audioError !== undefined && typeof tab.audioError !== 'string')) {
      throw new Error('Invalid native tab');
    }
    seen.add(tab.id);
  }
  const active = value.tabs.filter((tab: Tab) => tab.active);
  if (value.tabs.length > 0 && (active.length !== 1 || active[0].id !== value.activeTab)) {
    throw new Error('Inconsistent active tab');
  }
  return value as unknown as BrowserState;
}
