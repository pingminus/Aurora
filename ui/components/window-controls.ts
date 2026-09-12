import type { BrowserState, Command } from '../ipc/protocol.js';
import { icon, iconButton } from '../icons/icon.js';

export class WindowControls {
  private readonly maximize: HTMLButtonElement;
  private maximized: boolean | undefined;

  constructor(root: HTMLElement, dragRegion: HTMLElement, send: (command: Command) => void) {
    const minimize = iconButton('minimize', 'Minimize window', () => send('minimizeWindow'));
    this.maximize = iconButton('maximize', 'Maximize window', () => send('toggleMaximize'));
    const close = iconButton('close', 'Close window', () => send('closeWindow'));
    close.classList.add('window-close');
    root.append(minimize, this.maximize, close);
    dragRegion.style.setProperty('-webkit-app-region', 'drag');
  }

  render(state: BrowserState): void {
    const maximized = state.window?.maximized ?? false;
    if (maximized === this.maximized) return;
    this.maximized = maximized;
    const label = maximized ? 'Restore window' : 'Maximize window';
    this.maximize.title = label;
    this.maximize.setAttribute('aria-label', label);
    this.maximize.replaceChildren(icon(maximized ? 'restore' : 'maximize'));
  }
}
