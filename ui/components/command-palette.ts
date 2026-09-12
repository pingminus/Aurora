export interface PaletteCommand { id: string; title: string; shortcut?: string; execute: () => void }

export class CommandPalette {
  private readonly input = document.createElement('input');
  private readonly results = document.createElement('div');
  private previousFocus: HTMLElement | null = null;
  private filtered: PaletteCommand[] = [];
  private selected = 0;
  constructor(private readonly dialog: HTMLDialogElement, private readonly commands: () => PaletteCommand[]) {
    dialog.setAttribute('aria-label', 'Browser commands');
    this.input.placeholder = 'Find a command or switch a tab…';
    this.input.setAttribute('aria-label', 'Search browser commands');
    this.input.setAttribute('role', 'combobox');
    this.input.setAttribute('aria-controls', 'command-results');
    this.input.setAttribute('aria-autocomplete', 'list');
    this.results.id = 'command-results'; this.results.className = 'command-results';
    this.results.setAttribute('role', 'listbox'); this.results.setAttribute('aria-label', 'Commands');
    const close = document.createElement('button'); close.className = 'palette-close';
    close.textContent = 'Esc'; close.setAttribute('aria-label', 'Close commands');
    close.addEventListener('click', () => this.close());
    dialog.append(this.input, close, this.results);
    this.input.addEventListener('input', () => { this.selected = 0; this.render(); });
    this.input.addEventListener('keydown', (event) => {
      if (['ArrowDown', 'ArrowRight', 'ArrowUp', 'ArrowLeft'].includes(event.key)) {
        if ((event.key === 'ArrowRight' || event.key === 'ArrowLeft') && this.input.value) return;
        event.preventDefault();
        this.selected = Math.max(0, Math.min(this.filtered.length - 1,
          this.selected + (event.key === 'ArrowDown' || event.key === 'ArrowRight' ? 1 : -1)));
        this.renderSelection();
      } else if (event.key === 'Enter') { event.preventDefault(); this.run(this.filtered[this.selected]); }
    });
    dialog.addEventListener('close', () => { this.input.setAttribute('aria-expanded', 'false'); this.previousFocus?.focus(); });
  }
  open(): void {
    if (this.dialog.open) return;
    this.previousFocus = document.activeElement as HTMLElement;
    this.input.value = ''; this.selected = 0; this.render();
    this.input.setAttribute('aria-expanded', 'true'); this.dialog.showModal(); this.input.focus();
  }
  close(): void { this.dialog.close(); }
  private run(command?: PaletteCommand): void { if (command) { this.close(); command.execute(); } }
  private render(): void {
    this.filtered = this.commands().filter(command => command.title.toLowerCase().includes(this.input.value.toLowerCase()));
    this.results.replaceChildren();
    for (const [index, command] of this.filtered.entries()) {
      const button = document.createElement('button'); button.id = `command-${index}`;
      button.setAttribute('role', 'option'); button.tabIndex = -1;
      button.textContent = command.title;
      if (command.shortcut) { const key = document.createElement('kbd'); key.textContent = command.shortcut; button.append(key); }
      button.addEventListener('click', () => this.run(command)); this.results.append(button);
    }
    if (!this.filtered.length) { const empty = document.createElement('span'); empty.textContent = 'No matching commands'; this.results.append(empty); }
    this.renderSelection();
  }
  private renderSelection(): void {
    for (const [index, item] of [...this.results.children].entries()) item.setAttribute('aria-selected', String(index === this.selected));
    const selected = this.results.children[this.selected];
    if (selected && this.filtered.length) {
      this.input.setAttribute('aria-activedescendant', selected.id);
      selected.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    } else this.input.removeAttribute('aria-activedescendant');
  }
}
