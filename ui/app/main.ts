import { NativeClient } from '../ipc/client.js';
import type { BrowserState, Command } from '../ipc/protocol.js';
import { TabBar } from '../components/tab-bar.js';
import { CommandPalette, type PaletteCommand } from '../components/command-palette.js';
import { icon, iconButton } from '../icons/icon.js';
import { WindowControls } from '../components/window-controls.js';

const client = new NativeClient();
let state: BrowserState = { version: 1, tabs: [], activeTab: 0 };
const status = document.querySelector<HTMLElement>('#status')!;
const address = document.querySelector<HTMLInputElement>('#address')!;
let addressDirty = false;
const toolbar = document.querySelector<HTMLElement>('#navigation')!;
const tabBar = new TabBar(document.querySelector('#tabs')!, id => void send('activateTab', id), id => void send('closeTab', id),
  (id, muted) => void send('setTabMuted', id, undefined, { muted }),
  (id, volume) => void send('setTabVolume', id, undefined, { volume }));
const windowControls = new WindowControls(document.querySelector('#window-controls')!,
  document.querySelector('#window-drag-region')!, command => void send(command));

async function send(command: Command, tabId?: number, url?: string, audio: { muted?: boolean; volume?: number } = {}): Promise<void> {
  if (command === 'navigate' || command === 'activateTab') addressDirty = false;
  try {
    const response = await client.request({ version: 1, command, ...(tabId === undefined ? {} : { tabId }), ...(url === undefined ? {} : { url }), ...audio });
    if (response.activeTab !== state.activeTab) addressDirty = false;
    state = response; tabBar.render(state); windowControls.render(state);
    renderAddress();
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
const activeCommand = (command: Command): (() => void) => () => { void send(command, state.activeTab); };
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
    ...state.tabs.map(tab => ({ id: `tab-${tab.id}`, title: `Switch to ${tab.title || tab.url}`, execute: () => void send('activateTab', tab.id) })),
  ];
  return commands;
});

toolbar.prepend(iconButton('back', 'Back (Alt Left)', activeCommand('back')),
  iconButton('forward', 'Forward (Alt Right)', activeCommand('forward')),
  iconButton('reload', 'Reload (Ctrl R)', activeCommand('reload')));
document.querySelector('#new-tab')!.append(iconButton('plus', 'New tab (Ctrl T)', () => void send('createTab')));
document.querySelector('#commands')!.append(iconButton('command', 'Commands (Ctrl K)', () => palette.open()));
document.querySelector('#address-icon')!.append(icon('search'));
document.querySelector('#address-form')!.addEventListener('submit', event => {
  event.preventDefault(); const value = address.value.trim();
  if (value) { address.blur(); void send('navigate', state.activeTab, value); }
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
window.addEventListener('aurora-open-commands', () => palette.open());
async function poll(): Promise<void> {
  await send('state');
  window.setTimeout(() => void poll(), 500);
}
void poll();
