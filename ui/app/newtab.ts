import { parseFeed, type CveFeed, type CveEntry } from '../ipc/cve-protocol.js';
import { parseSessionPersistenceState, type SessionPersistenceState } from '../ipc/session-persistence-protocol.js';
const sessionSwitch = document.querySelector<HTMLButtonElement>('#save-sessions')!;
const sessionStatus = document.querySelector<HTMLElement>('#session-preference-status')!;
let sessionState: SessionPersistenceState | undefined;
function renderSessionState(state: SessionPersistenceState): void {
  sessionState = state;
  sessionSwitch.setAttribute('aria-checked', String(state.enabled));
  sessionSwitch.disabled = false;
  sessionStatus.textContent = state.enabled !== state.active ?
    'Preference saved. Restart OpenGod to apply this change.' :
    state.enabled ? 'On for this window.' : 'Off for this window.';
}
async function requestSessionState(enabled?: boolean): Promise<void> {
  sessionSwitch.disabled = true;
  try {
    const raw = await new Promise<string>((resolve, reject) => {
      if (!window.cefQuery) { reject(new Error('Available in OpenGod only.')); return; }
      let pending: number | undefined;
      const timeout = window.setTimeout(() => {
        if (pending !== undefined) window.cefQueryCancel?.(pending);
        reject(new Error('Preference request timed out.'));
      }, 5000);
      try {
        pending = window.cefQuery({request: JSON.stringify({version: 1,
          command: enabled === undefined ? 'sessionPersistenceState' : 'setSessionPersistence',
          ...(enabled === undefined ? {} : {enabled})}), persistent: false,
        onSuccess: response => { clearTimeout(timeout); resolve(response); },
        onFailure: () => { clearTimeout(timeout); reject(new Error('Could not save or load the preference.')); }});
      } catch (error) { clearTimeout(timeout); reject(error); }
    });
    renderSessionState(parseSessionPersistenceState(raw));
  } catch (error) {
    sessionSwitch.disabled = sessionState === undefined;
    sessionStatus.textContent = error instanceof Error ? error.message : 'Preference unavailable.';
  }
}
sessionSwitch.addEventListener('click', () => {
  if (sessionState) void requestSessionState(!sessionState.enabled);
});
void requestSessionState();
const status = document.querySelector<HTMLElement>('#feed-status')!;
const range = document.querySelector<HTMLElement>('#feed-range')!;
const updated = document.querySelector<HTMLElement>('#feed-updated')!;
const list = document.querySelector<HTMLOListElement>('#feed-list')!;
const refresh = document.querySelector<HTMLButtonElement>('#feed-refresh')!;
const more = document.querySelector<HTMLButtonElement>('#feed-more')!;
let current: CveFeed | undefined;
let limit = 50;
let signature = '';
let busy = false;
let disposed = false;
let pendingId: number | undefined;
function text<K extends keyof HTMLElementTagNameMap>(tag: K, value: string, className?: string): HTMLElementTagNameMap[K] {
  const element = document.createElement(tag); element.textContent = value;
  if (className) element.className = className; return element;
}
function date(value: string): string { return value ? value.replace('T', ' ').replace(/(\.\d+)?Z?$/, '') + ' UTC' : 'Not yet refreshed'; }
function card(entry: CveEntry): HTMLLIElement {
  const row = text('li', '', 'cve-card');
  const score = text('div', '', 'cve-score'); score.append(text('strong', entry.score === null ? '—' : entry.score.toFixed(1)), text('span', entry.severity), text('span', entry.score === null ? 'Unscored' : 'CVSS ' + entry.cvssVersion));
  const body = text('div', ''); const heading = text('h3', ''); const link = text('a', entry.id);
  // Native validation + a fixed authoritative origin; never accept API-provided URLs.
  link.href = 'https://nvd.nist.gov/vuln/detail/' + encodeURIComponent(entry.id); heading.append(link);
  const details = text('div', '', 'cve-details');
  details.append(text('span', 'Published ' + date(entry.published)), text('span', 'Updated ' + date(entry.modified)));
  if (entry.assessmentSource) details.append(text('span', 'Score source: ' + entry.assessmentSource));
  body.append(heading, text('p', entry.description), details); row.append(score, body); return row;
}
function renderEntries(): void {
  if (!current) return;
  list.replaceChildren(...current.entries.slice(0, limit).map(card));
  more.hidden = limit >= current.entries.length;
  more.textContent = 'Show more · ' + Math.min(limit, current.entries.length) + ' of ' + current.entries.length;
}
function render(feed: CveFeed): void {
  current = feed;
  const message = feed.status === 'ready' ? (feed.entries.length ? feed.entries.length + ' CVEs · CVSS descending · complete NVD response for the interval below.' : 'No published CVEs were returned for this interval.') :
    feed.status === 'stale' ? 'Showing cached results. ' + feed.message : feed.message || 'Loading today’s CVEs…';
  if (status.textContent !== message) status.textContent = message;
  status.dataset.status = feed.status; list.setAttribute('aria-busy', String(feed.status === 'loading'));
  range.textContent = (feed.start ? date(feed.start) + ' — ' + date(feed.end) : 'Midnight through now · UTC') + ' · NVD publication date';
  updated.textContent = feed.fetched ? 'Last successful refresh: ' + date(feed.fetched) : 'Not yet refreshed';
  refresh.disabled = feed.status === 'loading'; refresh.textContent = feed.status === 'error' || feed.status === 'stale' ? 'Retry' : 'Refresh';
  const nextSignature = feed.fetched + ':' + feed.entries.length;
  if (nextSignature !== signature) { signature = nextSignature; renderEntries(); }
}
async function request(command: 'cveState' | 'refreshCves'): Promise<void> {
  if (busy || disposed) return; busy = true;
  try {
    const raw = await new Promise<string>((resolve, reject) => {
      if (!window.cefQuery) { reject(new Error('Open this starting page in OpenGod to load the CVE feed.')); return; }
      const timeout = window.setTimeout(() => { if (pendingId !== undefined) window.cefQueryCancel?.(pendingId); pendingId = undefined; reject(new Error('The native feed did not respond. Retry to reconnect.')); }, 5000);
      try { pendingId = window.cefQuery({request: JSON.stringify({version: 1, command}), persistent: false,
        onSuccess: value => { clearTimeout(timeout); pendingId = undefined; resolve(value); },
        onFailure: () => { clearTimeout(timeout); pendingId = undefined; reject(new Error('The native feed request failed. Retry to reconnect.')); }});
      } catch (error) { clearTimeout(timeout); reject(error); }
    });
    if (!disposed) render(parseFeed(raw));
  } catch (error) {
    status.dataset.status = 'error'; status.textContent = (current?.entries.length ? 'Cached results remain below. ' : '') + (error instanceof Error ? error.message : 'Feed unavailable.'); refresh.disabled = false;
  } finally { busy = false; }
}
refresh.addEventListener('click', () => { void request('refreshCves'); });
more.addEventListener('click', () => { limit += 50; renderEntries(); });
const timer = window.setInterval(() => { if (!document.hidden) void request('cveState'); }, 3000);
window.addEventListener('pagehide', () => { disposed = true; clearInterval(timer); if (pendingId !== undefined) window.cefQueryCancel?.(pendingId); });
void request('cveState');

// Keep the exact privileged document URL unchanged when jumping to the feed.
document.querySelector<HTMLAnchorElement>('.scroll-cue')?.addEventListener('click', event => {
  event.preventDefault(); document.querySelector('#cve-feed')?.scrollIntoView();
});

const launchTerminal = document.querySelector<HTMLButtonElement>('#open-terminal')!;
launchTerminal.addEventListener('click', () => {
  const feedback = document.querySelector<HTMLElement>('#terminal-launch-status')!;
  if (!window.cefQuery) { feedback.textContent = 'Open this page in OpenGod to launch a terminal.'; return; }
  launchTerminal.disabled = true;
  const timeout = window.setTimeout(() => { launchTerminal.disabled = false; feedback.textContent = 'Terminal creation did not respond. Check your tabs before retrying.'; }, 5000);
  window.cefQuery({request: JSON.stringify({version: 1, command: 'createTerminal'}), persistent: false,
    onSuccess: () => { clearTimeout(timeout); launchTerminal.disabled = false; feedback.textContent = ''; },
    onFailure: () => { clearTimeout(timeout); launchTerminal.disabled = false; feedback.textContent = 'Terminal could not be opened.'; }});
});

const launchOmniroute = document.querySelector<HTMLButtonElement>('#open-omniroute')!;
launchOmniroute.addEventListener('click', () => {
  const feedback = document.querySelector<HTMLElement>('#terminal-launch-status')!;
  if (!window.cefQuery) { feedback.textContent = 'Open this page in OpenGod to start omniroute.'; return; }
  launchOmniroute.disabled = true;
  const timeout = window.setTimeout(() => { launchOmniroute.disabled = false; feedback.textContent = 'Omniroute launch did not respond. Check your tabs before retrying.'; }, 5000);
  window.cefQuery({request: JSON.stringify({version: 1, command: 'launchOmniroute'}), persistent: false,
    onSuccess: () => { clearTimeout(timeout); launchOmniroute.disabled = false; feedback.textContent = ''; },
    onFailure: () => { clearTimeout(timeout); launchOmniroute.disabled = false; feedback.textContent = 'Omniroute could not be started.'; }});
});

interface Macro { id: number; name: string; terminalCommand: string; url: string; }
type MacroSnapshot = { version: 1; macros: Macro[] };
let macros: Macro[] = [];
let editingMacroId: number | undefined;
const dialog = document.querySelector<HTMLDialogElement>('#macro-editor')!;
const form = document.querySelector<HTMLFormElement>('#macro-form')!;
const nameInput = document.querySelector<HTMLInputElement>('#macro-name')!;
const hasTerminal = document.querySelector<HTMLInputElement>('#macro-has-terminal')!;
const terminalCmd = document.querySelector<HTMLInputElement>('#macro-terminal-command')!;
const hasUrl = document.querySelector<HTMLInputElement>('#macro-has-url')!;
const urlInput = document.querySelector<HTMLInputElement>('#macro-url')!;
const contextMenu = document.querySelector<HTMLElement>('#macro-context-menu')!;
const launchRow = document.querySelector<HTMLElement>('#launch-row')!;
const feedback = document.querySelector<HTMLElement>('#terminal-launch-status')!;

function loadMacros(): Promise<MacroSnapshot> {
  return new Promise((resolve, reject) => {
    if (!window.cefQuery) { reject(new Error('OpenGod not available')); return; }
    const timeout = window.setTimeout(() => { reject(new Error('Macro state request timed out')); }, 5000);
    window.cefQuery({request: JSON.stringify({version: 1, command: 'macroState'}), persistent: false,
      onSuccess: (raw) => { clearTimeout(timeout); resolve(JSON.parse(raw) as MacroSnapshot); },
      onFailure: () => { clearTimeout(timeout); reject(new Error('Failed to load macros')); }});
  });
}

function saveMacro(name: string, terminalCommand: string, url: string, macroId?: number): Promise<MacroSnapshot> {
  return new Promise((resolve, reject) => {
    if (!window.cefQuery) { reject(new Error('OpenGod not available')); return; }
    const req: any = {version: 1, command: 'saveMacro', name, terminalCommand, url};
    if (macroId) req.macroId = macroId;
    const timeout = window.setTimeout(() => { reject(new Error('Save macro request timed out')); }, 5000);
    window.cefQuery({request: JSON.stringify(req), persistent: false,
      onSuccess: (raw) => { clearTimeout(timeout); resolve(JSON.parse(raw) as MacroSnapshot); },
      onFailure: (_code, msg) => { clearTimeout(timeout); reject(new Error(msg || 'Save macro failed')); }});
  });
}

function deleteMacro(macroId: number): Promise<MacroSnapshot> {
  return new Promise((resolve, reject) => {
    if (!window.cefQuery) { reject(new Error('OpenGod not available')); return; }
    const timeout = window.setTimeout(() => { reject(new Error('Delete macro request timed out')); }, 5000);
    window.cefQuery({request: JSON.stringify({version: 1, command: 'deleteMacro', macroId}), persistent: false,
      onSuccess: (raw) => { clearTimeout(timeout); resolve(JSON.parse(raw) as MacroSnapshot); },
      onFailure: (_code, msg) => { clearTimeout(timeout); reject(new Error(msg || 'Delete macro failed')); }});
  });
}

function launchMacro(macroId: number): Promise<void> {
  return new Promise((resolve, reject) => {
    if (!window.cefQuery) { reject(new Error('OpenGod not available')); return; }
    const timeout = window.setTimeout(() => { reject(new Error('Launch macro request timed out')); }, 5000);
    window.cefQuery({request: JSON.stringify({version: 1, command: 'launchMacro', macroId}), persistent: false,
      onSuccess: () => { clearTimeout(timeout); resolve(); },
      onFailure: (_code, msg) => { clearTimeout(timeout); reject(new Error(msg || 'Launch macro failed')); }});
  });
}

function renderMacros(snapshot: MacroSnapshot): void {
  macros = snapshot.macros;
  const existing = launchRow.querySelectorAll<HTMLButtonElement>('.macro-button');
  existing.forEach(btn => btn.remove());
  for (const macro of macros) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'feed-button terminal-launch macro-button';
    btn.textContent = macro.name;
    btn.addEventListener('click', () => { void launchMacro(macro.id).catch(e => { feedback.textContent = String(e); }); });
    btn.addEventListener('contextmenu', (e) => {
      e.preventDefault();
      contextMenu.hidden = false;
      contextMenu.style.left = e.pageX + 'px';
      contextMenu.style.top = e.pageY + 'px';
      const editBtn = contextMenu.querySelector<HTMLElement>('#macro-edit')!;
      const deleteBtn = contextMenu.querySelector<HTMLElement>('#macro-delete')!;
      editBtn.onclick = () => { editingMacroId = macro.id; nameInput.value = macro.name; hasTerminal.checked = !!macro.terminalCommand; terminalCmd.value = macro.terminalCommand; hasUrl.checked = !!macro.url; urlInput.value = macro.url; dialog.querySelector<HTMLElement>('#macro-editor-title')!.textContent = 'Edit macro'; terminalCmd.disabled = !hasTerminal.checked; urlInput.disabled = !hasUrl.checked; dialog.showModal(); contextMenu.hidden = true; };
      deleteBtn.onclick = () => { if (window.confirm('Delete this macro?')) { void deleteMacro(macro.id).then(renderMacros).catch(e => { feedback.textContent = String(e); }); } contextMenu.hidden = true; };
    });
    launchRow.insertBefore(btn, launchRow.querySelector('#add-macro-btn'));
  }
}

hasTerminal.addEventListener('change', () => { terminalCmd.disabled = !hasTerminal.checked; if (hasTerminal.checked) terminalCmd.focus(); });
hasUrl.addEventListener('change', () => { urlInput.disabled = !hasUrl.checked; if (hasUrl.checked) urlInput.focus(); });

document.querySelector<HTMLButtonElement>('#add-macro-btn')!.addEventListener('click', () => {
  editingMacroId = undefined;
  nameInput.value = '';
  hasTerminal.checked = false;
  terminalCmd.value = '';
  terminalCmd.disabled = true;
  hasUrl.checked = false;
  urlInput.value = '';
  urlInput.disabled = true;
  dialog.querySelector<HTMLElement>('#macro-editor-title')!.textContent = 'New macro';
  dialog.showModal();
});

form.addEventListener('submit', async (e) => {
  e.preventDefault();
  const name = nameInput.value.trim();
  const terminalCommand = hasTerminal.checked ? terminalCmd.value.trim() : '';
  const url = hasUrl.checked ? urlInput.value.trim() : '';
  if (!name) { feedback.textContent = 'Name is required'; return; }
  if (!terminalCommand && !url) { feedback.textContent = 'Macro must have a terminal command or URL'; return; }
  try {
    const result = await saveMacro(name, terminalCommand, url, editingMacroId);
    renderMacros(result);
    dialog.close();
    feedback.textContent = '';
  } catch (e) {
    feedback.textContent = String(e);
  }
});

document.querySelector<HTMLButtonElement>('#macro-cancel')!.addEventListener('click', () => { dialog.close(); });
dialog.querySelector<HTMLButtonElement>('.macro-editor-close')!.addEventListener('click', () => { dialog.close(); });

document.addEventListener('click', (e) => {
  if (!contextMenu.contains(e.target as Node) && e.target !== launchRow) contextMenu.hidden = true;
});

 void loadMacros().then(renderMacros).catch(e => { console.error('Failed to load macros:', e); });

// White screen overlay toggle
const whiteScreenToggle = document.querySelector<HTMLButtonElement>('#white-screen')!;
const whiteScreenStatus = document.querySelector<HTMLElement>('#white-screen-status')!;
const whiteScreenOverlay = document.createElement('div');
whiteScreenOverlay.className = 'white-screen-overlay';
whiteScreenOverlay.hidden = true;
document.body.appendChild(whiteScreenOverlay);

function loadWhiteScreenState(): boolean {
  const saved = localStorage.getItem('opengod-white-screen');
  return saved === 'true';
}

function saveWhiteScreenState(enabled: boolean): void {
  localStorage.setItem('opengod-white-screen', String(enabled));
}

function renderWhiteScreenState(enabled: boolean): void {
  whiteScreenToggle.setAttribute('aria-checked', String(enabled));
  whiteScreenOverlay.hidden = !enabled;
  whiteScreenStatus.textContent = enabled ? 'On · overlay active' : 'Off';
}

function toggleWhiteScreen(): void {
  const current = whiteScreenToggle.getAttribute('aria-checked') === 'true';
  const next = !current;
  saveWhiteScreenState(next);
  renderWhiteScreenState(next);
}

// Initialize white screen state
renderWhiteScreenState(loadWhiteScreenState());
whiteScreenToggle.addEventListener('click', toggleWhiteScreen);
