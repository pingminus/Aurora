import { NativeClient } from '../ipc/client.js';
import type { BrowserState, Command } from '../ipc/protocol.js';
import { TabBar } from '../components/tab-bar.js';
import { CommandPalette, type PaletteCommand } from '../components/command-palette.js';
import { icon, iconButton } from '../icons/icon.js';
import { WindowControls } from '../components/window-controls.js';
import { BookmarkBar } from '../components/bookmark-bar.js';

interface SendOptions { tabId?: number; url?: string; muted?: boolean; volume?: number; bookmarkId?: number }

const client = new NativeClient();
let state: BrowserState = { version: 1, tabs: [], activeTab: 0, bookmarks: [] };
const status = document.querySelector<HTMLElement>('#status')!;
const address = document.querySelector<HTMLInputElement>('#address')!;
let addressDirty = false;
const toolbar = document.querySelector<HTMLElement>('#navigation')!;
const tabBar = new TabBar(document.querySelector('#tabs')!, id => void send('activateTab', { tabId: id }), id => void send('closeTab', { tabId: id }),
  (id, muted) => void send('setTabMuted', { tabId: id, muted }),
  (id, volume) => void send('setTabVolume', { tabId: id, volume }));
const windowControls = new WindowControls(document.querySelector('#window-controls')!,
  document.querySelector('#window-drag-region')!, command => void send(command));
const bookmarkBar = new BookmarkBar(document.querySelector('#bookmark-bar')!,
  url => void send('navigate', { tabId: state.activeTab, url }),
  id => void send('removeBookmark', { bookmarkId: id }));

async function send(command: Command, options: SendOptions = {}): Promise<void> {
  if (command === 'navigate' || command === 'activateTab') addressDirty = false;
  try {
    const response = await client.request({ version: 1, command, ...options });
    if (response.activeTab !== state.activeTab) addressDirty = false;
    state = response; tabBar.render(state); bookmarkBar.render(state); windowControls.render(state);
    renderAddress(); renderBookmarkButton();
    if (command === 'createTab' && state.tabs.find(tab => tab.id === state.activeTab)?.url.startsWith('aurora://newtab'))
      focusAddress();
    status.hidden = true; document.body.classList.remove('disconnected');
  } catch (error) {
    status.textContent = error instanceof Error ? error.message : 'Native browser request failed';
    status.hidden = false; document.body.classList.add('disconnected');
  }
}

function renderAddress(): void {
  if (addressDirty) return;
  const active = state.tabs.find(tab => tab.active);
  const value = /^aurora:\/\/newtab\/?$/.test(active?.url ?? '') ? '' : (active?.url ?? '');
  // Avoid resetting the caret/selection on every poll when the URL is unchanged.
  if (address.value !== value) address.value = value;
}
const endAddressEdit = (): void => { addressDirty = false; renderAddress(); };
address.addEventListener('input', () => { addressDirty = true; });
address.addEventListener('blur', endAddressEdit);
// CEF's separate content view can take native focus while activeElement stays the input.
window.addEventListener('blur', endAddressEdit);

const focusAddress = (): void => { address.focus(); address.select(); };
const activeCommand = (command: Command): (() => void) => () => { void send(command, { tabId: state.activeTab }); };
const palette = new CommandPalette(document.querySelector('#palette')!, () => {
  const commands: PaletteCommand[] = [
    { id: 'new', title: 'New tab', shortcut: 'Ctrl T', execute: () => void send('createTab') },
    { id: 'address', title: 'Focus address bar', shortcut: 'Ctrl L', execute: focusAddress },
    { id: 'close', title: 'Close tab', shortcut: 'Ctrl W', execute: activeCommand('closeTab') },
    { id: 'duplicate', title: 'Duplicate tab', execute: activeCommand('duplicateTab') },
    { id: 'reopen', title: 'Reopen closed tab', shortcut: 'Ctrl Shift T', execute: () => void send('reopenTab') },
    { id: 'reload', title: 'Reload page', shortcut: 'Ctrl R', execute: activeCommand('reload') },
    { id: 'devtools', title: 'Open developer tools', execute: activeCommand('devtools') },
    ...(['dark'] as const).map(theme => ({ id: `theme-${theme}`, title: `Theme: ${theme}`, execute: () => {
      document.documentElement.dataset.theme = theme;
    } })),
    ...state.tabs.map(tab => ({ id: `tab-${tab.id}`, title: `Switch to ${tab.title || tab.url}`, execute: () => void send('activateTab', { tabId: tab.id }) })),
    ...state.bookmarks.map(bookmark => ({ id: `bookmark-${bookmark.id}`, title: `Open ${bookmark.title || bookmark.url}`, execute: () => void send('navigate', { tabId: state.activeTab, url: bookmark.url }) })),
  ];
  return commands;
});

const bookmark = iconButton('bookmark', 'Bookmark the current page', () => void send('addBookmark', { tabId: state.activeTab }));
bookmark.setAttribute('aria-pressed', 'false');
function renderBookmarkButton(): void {
  const activeUrl = state.tabs.find(tab => tab.active)?.url;
  const saved = activeUrl !== undefined && state.bookmarks.some(item => item.url === activeUrl);
  bookmark.setAttribute('aria-pressed', String(saved));
  const label = saved ? 'Bookmark the current page (saved)' : 'Bookmark the current page';
  bookmark.title = saved ? 'Saved — remove it from the bookmark bar' : 'Bookmark the current page';
  bookmark.setAttribute('aria-label', label);
}

toolbar.prepend(iconButton('back', 'Back (Alt Left)', activeCommand('back')),
  iconButton('forward', 'Forward (Alt Right)', activeCommand('forward')),
  iconButton('reload', 'Reload (Ctrl R)', activeCommand('reload')));
bookmark.id = 'bookmark-page';
toolbar.insertBefore(bookmark, document.querySelector('#commands'));
renderBookmarkButton();
document.querySelector('#new-tab')!.append(iconButton('plus', 'New tab (Ctrl T)', () => void send('createTab')));
document.querySelector('#commands')!.append(iconButton('command', 'Commands (Ctrl K)', () => palette.open()));
document.querySelector('#address-icon')!.append(icon('search'));
document.querySelector('#address-form')!.addEventListener('submit', event => {
  event.preventDefault(); const value = address.value.trim();
  if (value) { address.blur(); void send('navigate', { tabId: state.activeTab, url: value }); }
});
address.addEventListener('keydown', event => {
  if (event.key === 'Escape') { address.blur(); void send('state'); }
});

document.addEventListener('keydown', event => {
  const key = event.key.toLowerCase();
  if (event.ctrlKey || event.metaKey) {
    if (key === 'l') { event.preventDefault(); focusAddress(); }
    else if (key === 'k') { event.preventDefault(); palette.open(); }
    else if (key === 't') { event.preventDefault(); void send(event.shiftKey ? 'reopenTab' : 'createTab'); }
    else if (key === 'w') { event.preventDefault(); activeCommand('closeTab')(); }
    else if (key === 'r') { event.preventDefault(); activeCommand('reload')(); }
  } else if (event.altKey && event.key === 'ArrowLeft') { event.preventDefault(); activeCommand('back')(); }
  else if (event.altKey && event.key === 'ArrowRight') { event.preventDefault(); activeCommand('forward')(); }
});

// Native host can focus the shell and dispatch this event for shortcuts originating in web content.
window.addEventListener('aurora-focus-address', focusAddress);
window.addEventListener('aurora-new-tab-address', event => {
  const tabId: unknown = (event as CustomEvent<unknown>).detail;
  if (typeof tabId !== 'number') return;
  // Creation is asynchronous: refresh the native snapshot before selecting
  // the address, and ignore callbacks for tabs the user has already left.
  void send('state').then(() => {
    const tab = state.tabs.find(candidate => candidate.id === tabId);
    if (state.activeTab === tabId && tab && !tab.terminal) focusAddress();
  });
});
window.addEventListener('aurora-open-commands', () => palette.open());
async function poll(): Promise<void> {
  await send('state');
  window.setTimeout(() => void poll(), 500);
}
void poll();
