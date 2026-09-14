import type { BrowserState } from "../ipc/protocol.js";
import { icon, iconButton } from "../icons/icon.js";

export class BookmarkBar {
  private readonly nodes = new Map<number, HTMLElement>();
  private readonly empty: HTMLElement;
  private editor: { id: number; form: HTMLFormElement; close: () => void } | undefined;
  constructor(
    private readonly root: HTMLElement,
    private readonly open: (url: string) => void,
    private readonly remove: (id: number) => void,
    private readonly rename: (id: number, title: string) => Promise<boolean>,
  ) {
    this.empty = document.createElement("span");
    this.empty.className = "bookmark-empty";
    this.empty.textContent =
      "No bookmarks yet — use the star to save the current page.";
  }

  private startRename(id: number, title: string, trigger: HTMLButtonElement): void {
    if (this.editor) {
      this.editor.form.querySelector('input')?.focus();
      return;
    }
    const form = document.createElement('form');
    form.className = 'bookmark-editor';
    const input = document.createElement('input');
    input.type = 'text';
    input.value = title;
    input.required = true;
    input.setAttribute('aria-label', 'Bookmark title');
    const save = document.createElement('button');
    save.type = 'submit';
    save.textContent = 'Save';
    const cancel = document.createElement('button');
    cancel.type = 'button';
    cancel.textContent = 'Cancel';
    const feedback = document.createElement('span');
    feedback.setAttribute('role', 'status');
    let saving = false;
    const close = (): void => {
      form.remove();
      if (this.editor?.form === form) this.editor = undefined;
      if (trigger.isConnected) trigger.focus();
    };
    cancel.addEventListener('click', close);
    form.addEventListener('keydown', event => {
      if (event.key === 'Escape') {
        event.preventDefault();
        event.stopPropagation();
        if (!saving) close();
      }
    });
    form.addEventListener('submit', async event => {
      event.preventDefault();
      if (saving) return;
      saving = true;
      input.disabled = save.disabled = cancel.disabled = true;
      feedback.textContent = 'Saving…';
      const success = await this.rename(id, input.value);
      // A newer snapshot may have removed this bookmark during the request.
      if (this.editor?.form !== form) return;
      if (success) {
        close();
      } else {
        saving = false;
        input.disabled = save.disabled = cancel.disabled = false;
        feedback.textContent = 'Not saved. Check the title and retry.';
        input.focus();
      }
    });
    form.append(input, save, cancel, feedback);
    this.editor = { id, form, close };
    this.root.prepend(form);
    input.focus();
    input.select();
  }

  render(state: BrowserState): void {
    const ids = new Set(state.bookmarks.map((bookmark) => bookmark.id));
    for (const [id, node] of this.nodes)
      if (!ids.has(id)) {
        node.remove();
        this.nodes.delete(id);
      }
    if (this.editor && !ids.has(this.editor.id)) this.editor.close();
    if (state.bookmarks.length === 0) {
      if (!this.empty.isConnected) this.root.append(this.empty);
    } else if (this.empty.isConnected) {
      this.empty.remove();
    }
    for (const bookmark of state.bookmarks) {
      let node = this.nodes.get(bookmark.id);
      if (!node) {
        node = document.createElement("div");
        node.className = "bookmark";
        const open = document.createElement("button");
        open.type = "button";
        open.className = "bookmark-open";
        open.append(icon("bookmark"), document.createElement("span"));
        open.addEventListener("click", () => this.open(bookmark.url));
        const remove = iconButton("close", "Remove bookmark", () =>
          this.remove(bookmark.id),
        );
        remove.classList.add("bookmark-remove");
        const edit = document.createElement('button');
        edit.type = 'button';
        edit.className = 'bookmark-edit';
        edit.textContent = 'Edit';
        node.append(open, edit, remove);
        this.nodes.set(bookmark.id, node);
        this.root.append(node);
      }
      const title = bookmark.title || bookmark.url || "Untitled";
      const open = node.querySelector<HTMLButtonElement>(".bookmark-open")!;
      open.querySelector("span")!.textContent = title;
      open.title = `${title} — ${bookmark.url}`;
      const edit = node.querySelector<HTMLButtonElement>('.bookmark-edit')!;
      edit.setAttribute('aria-label', `Rename ${title}`);
      edit.onclick = () => this.startRename(bookmark.id, bookmark.title, edit);
      node
        .querySelector(".bookmark-remove")!
        .setAttribute("aria-label", `Remove ${title}`);
    }
  }
}
