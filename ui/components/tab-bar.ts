import type { BrowserState } from '../ipc/protocol.js';
import { icon, iconButton } from '../icons/icon.js';

export class TabBar {
  private readonly nodes = new Map<number, HTMLElement>();
  constructor(private readonly root: HTMLElement, private readonly activate: (id: number) => void,
    private readonly close: (id: number) => void,
    private readonly mute: (id: number, muted: boolean) => void,
    private readonly volume: (id: number, volume: number) => void) {
    root.addEventListener('keydown', (event) => {
      const tabs = [...root.querySelectorAll<HTMLButtonElement>('[role="tab"]')];
      const current = tabs.indexOf(document.activeElement as HTMLButtonElement);
      if (current < 0) return;
      let next = current;
      if (event.key === 'ArrowRight') next = (current + 1) % tabs.length;
      else if (event.key === 'ArrowLeft') next = (current - 1 + tabs.length) % tabs.length;
      else if (event.key === 'Home') next = 0;
      else if (event.key === 'End') next = tabs.length - 1;
      else if (event.key === 'Delete') { event.preventDefault(); this.close(Number(tabs[current]?.dataset.id)); return; }
      else return;
      event.preventDefault(); tabs[next]?.focus(); tabs[next]?.click();
    });
  }

  render(state: BrowserState): void {
    const ids = new Set(state.tabs.map(tab => tab.id));
    for (const [id, node] of this.nodes) if (!ids.has(id)) { node.remove(); this.nodes.delete(id); }
    for (const tab of state.tabs) {
      let node = this.nodes.get(tab.id);
      if (!node) {
        node = document.createElement('div'); node.className = 'tab';
        const select = document.createElement('button'); select.type = 'button';
        select.className = 'tab-select'; select.setAttribute('role', 'tab'); select.dataset.id = String(tab.id);
        select.append(icon('globe'), document.createElement('span'));
        select.addEventListener('click', () => this.activate(tab.id));
        const mute = iconButton('speaker', 'Mute tab', () => this.mute(tab.id, mute.getAttribute('aria-pressed') !== 'true'));
        mute.classList.add('tab-mute');
        const slider = document.createElement('input');
        slider.type = 'range'; slider.min = '0'; slider.max = '100'; slider.step = '1'; slider.className = 'tab-volume';
        slider.addEventListener('input', () => {
          slider.dataset.editing = 'true';
          slider.setAttribute('aria-valuetext', `${slider.value}%`);
          this.volume(tab.id, Number(slider.value));
        });
        slider.addEventListener('change', () => { delete slider.dataset.editing; });
        slider.addEventListener('blur', () => { delete slider.dataset.editing; });
        const close = iconButton('close', 'Close tab', () => this.close(tab.id)); close.classList.add('tab-close');
        node.append(select, mute, slider, close);
        this.nodes.set(tab.id, node); this.root.append(node);
      }
      const select = node.querySelector<HTMLButtonElement>('.tab-select')!;
      const title = tab.title || (/^opengod:\/\/newtab\/?$/.test(tab.url) ? 'New tab' : tab.url) || 'Untitled';
      select.querySelector('span')!.textContent = title;
      select.title = title; select.setAttribute('aria-selected', String(tab.active));
      select.tabIndex = tab.active ? 0 : -1;
      node.classList.toggle('active', tab.active);
      node.querySelector('.tab-close')!.setAttribute('aria-label', `Close ${title}`);
      const mute = node.querySelector<HTMLButtonElement>('.tab-mute')!;
      const label = `${tab.muted ? 'Unmute' : 'Mute'} ${title}`;
      if (mute.getAttribute('aria-pressed') !== String(tab.muted)) mute.replaceChildren(icon(tab.muted ? 'muted' : 'speaker'));
      mute.setAttribute('aria-pressed', String(tab.muted)); mute.setAttribute('aria-label', label);
      mute.hidden = Boolean(tab.terminal);
      mute.title = tab.audioError || label;
      node.classList.toggle('audio-error', Boolean(tab.audioError));
      const slider = node.querySelector<HTMLInputElement>('.tab-volume')!;
      slider.hidden = Boolean(tab.terminal);
      if (!slider.dataset.editing) slider.value = String(tab.volume);
      slider.setAttribute('aria-label', `Volume for ${title}`);
      slider.setAttribute('aria-valuetext', `${slider.value}%${tab.muted ? ', muted' : ''}`);
      slider.title = tab.audioError || `Volume ${slider.value}%${tab.muted ? ' (muted)' : ''}`;
    }
  }
}
