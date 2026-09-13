const paths = {
  speaker: 'M11 4 6 8H3v8h3l5 4zM15 8a6 6 0 0 1 0 8M18 5a10 10 0 0 1 0 14',
  muted: 'M11 4 6 8H3v8h3l5 4zM16 9l6 6M22 9l-6 6',
  back: 'm14 6-6 6 6 6M8 12h12', forward: 'm10 6 6 6-6 6M4 12h12',
  reload: 'M20 8a8 8 0 1 0 0 8M20 3v5h-5', plus: 'M12 5v14M5 12h14',
  close: 'm7 7 10 10M7 17 17 7', search: 'M20 20l-5-5M17 10a7 7 0 1 1-14 0 7 7 0 0 1 14 0',
  globe: 'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0M3 12h18M12 3c5 5 5 13 0 18-5-5-5-13 0-18',
  command: 'M9 9H6a3 3 0 1 1 3-3v12a3 3 0 1 1-3-3h12a3 3 0 1 1-3 3V6a3 3 0 1 1 3 3H9',
  bookmark: 'm12 3 2.7 5.5 6 .9-4.35 4.2 1.03 6L12 16.8 6.62 19.6l1.03-6L3.3 9.4l6-.9z',
  minimize: 'M5 12h14', maximize: 'M5 5h14v14H5z', restore: 'M8 8h11v11H8zM5 15V5h10',
} as const;
export function icon(name: keyof typeof paths): SVGSVGElement {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('aria-hidden', 'true');
  const path = document.createElementNS(svg.namespaceURI, 'path');
  path.setAttribute('d', paths[name]);
  svg.append(path);
  return svg;
}
export function iconButton(name: keyof typeof paths, label: string, action: () => void): HTMLButtonElement {
  const button = document.createElement('button');
  button.type = 'button'; button.className = 'icon-button';
  button.title = label; button.setAttribute('aria-label', label);
  button.append(icon(name)); button.addEventListener('click', action);
  return button;
}
