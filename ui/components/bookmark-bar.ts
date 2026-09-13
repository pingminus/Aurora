import type { BrowserState } from '../ipc/protocol.js';
import { icon, iconButton } from '../icons/icon.js';

export class BookmarkBar {
  private readonly nodes = new Map<number, HTMLElement>();
  private readonly empty: HTMLElement;
  constructor(private readonly root: HTMLElement, private readonly open: (url: string) => void,
    private readonly remove: (id: number) => void) {
    this.empty = document.createElement('span');
    this.empty.className = 'bookmark-empty';
    this.empty.textContent = 'No bookmarks yet — use the star to save the current page.';
  }

  render(state: BrowserState): void {
    const ids = new Set(state.bookmarks.map(bookmark => bookmark.id));
    for (const [id, node] of this.nodes) if (!ids.has(id)) { node.remove(); this.nodes.delete(id); }
    if (state.bookmarks.length === 0) {
      if (!this.empty.isConnected) this.root.append(this.empty);
    } else if (this.empty.isConnected) {
      this.empty.remove();
    }
    for (const bookmark of state.bookmarks) {
      let node = this.nodes.get(bookmark.id);
      if (!node) {
        node = document.createElement('div'); node.className = 'bookmark';
        const open = document.createElement('button'); open.type = 'button';
        open.className = 'bookmark-open';
        open.append(icon('bookmark'), document.createElement('span'));
        open.addEventListener('click', () => this.open(bookmark.url));
        const remove = iconButton('close', 'Remove bookmark', () => this.remove(bookmark.id));
        remove.classList.add('bookmark-remove');
        node.append(open, remove);
        this.nodes.set(bookmark.id, node); this.root.append(node);
      }
      const title = bookmark.title || bookmark.url || 'Untitled';
      const open = node.querySelector<HTMLButtonElement>('.bookmark-open')!;
      open.querySelector('span')!.textContent = title;
      open.title = `${title} — ${bookmark.url}`;
      node.querySelector('.bookmark-remove')!.setAttribute('aria-label', `Remove ${title}`);
    }
  }
}