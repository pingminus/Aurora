import { copyFile, mkdir } from 'node:fs/promises';
for (const directory of ['styles', 'theme', 'vendor', 'icons']) await mkdir(`ui/dist/${directory}`, { recursive: true });
for (const file of ['icons/opengod.svg', 'icons/opengod.ico', 'index.html', 'newtab.html', 'terminal.html', 'styles/terminal.css', 'styles/shell.css', 'styles/newtab.css', 'theme/tokens.css']) {
  await copyFile(`ui/${file}`, `ui/dist/${file}`);
}

for (const [source,target] of [
 ['@xterm/xterm/lib/xterm.js','xterm.js'],['@xterm/xterm/css/xterm.css','xterm.css'],['@xterm/xterm/LICENSE','xterm-LICENSE.txt'],
 ['@xterm/addon-fit/lib/addon-fit.js','addon-fit.js'],['@xterm/addon-fit/LICENSE','addon-fit-LICENSE.txt']]) await copyFile('node_modules/'+source,'ui/dist/vendor/'+target);
