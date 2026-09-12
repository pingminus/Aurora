# UI engineering

The native browser is authoritative. Components render validated IPC snapshots; never invent tabs, navigation success, permission state, or web content. The shell is a dedicated 112 CSS pixel native view; content is a separate CEF browser. Never use iframes for web pages.

Keep TypeScript strict. Use textContent for website-controlled titles and URLs. Protocol changes require coordinated C++ validation and protocol tests. The exact internal new-tab main frame may use only the read-only CVE feed capability authorized by the user. Never expose shell commands there or native APIs to remote sites. Every visible action must execute an implemented command; put future features in the roadmap.

Use shared tokens, SVG icons, semantic controls, visible focus, keyboard navigation and reduced-motion styles. Theme commands are presentation-only and currently last for the shell lifetime. Native settings persistence must own future saved theme preference. New-tab search is an ordinary GET form; provider configuration belongs to native settings in the next milestone.

Run `npm ci`, `npm run typecheck`, and `npm test`. Inspect the shell at its actual 112px height and the new-tab page in light/dark themes. Do not confuse standalone UI inspection with a successful native browser smoke test.
