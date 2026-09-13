import { parseFeed, type CveFeed, type CveEntry } from '../ipc/cve-protocol.js';
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
      if (!window.cefQuery) { reject(new Error('Open this starting page in Aurora to load the CVE feed.')); return; }
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
  if (!window.cefQuery) { feedback.textContent = 'Open this page in Aurora to launch a terminal.'; return; }
  launchTerminal.disabled = true;
  const timeout = window.setTimeout(() => { launchTerminal.disabled = false; feedback.textContent = 'Terminal creation did not respond. Check your tabs before retrying.'; }, 5000);
  window.cefQuery({request: JSON.stringify({version: 1, command: 'createTerminal'}), persistent: false,
    onSuccess: () => { clearTimeout(timeout); launchTerminal.disabled = false; feedback.textContent = ''; },
    onFailure: () => { clearTimeout(timeout); launchTerminal.disabled = false; feedback.textContent = 'Terminal could not be opened.'; }});
});
