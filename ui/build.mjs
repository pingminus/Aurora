import { copyFile, mkdir } from 'node:fs/promises';
for (const directory of ['styles', 'theme']) await mkdir(`ui/dist/${directory}`, { recursive: true });
for (const file of ['index.html', 'newtab.html', 'styles/shell.css', 'styles/newtab.css', 'theme/tokens.css']) {
  await copyFile(`ui/${file}`, `ui/dist/${file}`);
}
