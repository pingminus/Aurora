# Interface system

OPENGOD uses restrained translucent surfaces, a quiet ambient background, generous spacing and a coherent vector icon family. The shell must remain readable with no website open and when transparency, motion or high-performance GPU effects are unavailable.

Tokens cover background/foreground, accents, surface opacity, blur, borders, shadows, radii, spacing, typography, duration and easing. Theme changes modify tokens rather than component-local colors. Light, dark and system themes must share semantic structure. Use opaque fallbacks and sufficient contrast; blur cannot substitute for a readable background.

The trusted shell is a separate native CEF surface from web content. CSS effects cannot automatically blur or layer over the web child window. Design menus, palette and panel placement around the real native composition boundary. Do not use a website iframe as a workaround.

## Components and interaction

Keep navigation controls, address bar, tabs, command palette and transient feedback in focused components. C++ snapshots drive tab content; local state tracks focus, open menus and animation. Use shared spacing and icon geometry. Only expose commands that have working native effects. Pending product capabilities belong in the roadmap, not inert buttons.

Omnibox input must distinguish navigation from search using native policy. Do not split policy between JavaScript and C++. Search providers belong behind configuration. Future suggestion categories should identify history, bookmarks, open tabs and commands clearly.

Animate transform and opacity for short transitions; constrain blur and avoid continuous expensive background work. Honor `prefers-reduced-motion`. Check small windows, high DPI and long tab titles. Keyboard shortcuts must work when website content has focus through deliberate native handling, not only shell DOM listeners.

## Accessibility

Use semantic buttons and labels, visible focus, logical tab order and explicit selected-tab state. Palette/menu dismissal restores focus to the invoking control. Ensure sufficient target sizes, zoom-friendly dimensions, high-contrast support and keyboard reachability. Check with Windows screen-reader tooling in the native application; a browser preview alone does not cover native child-window focus behavior.

## OpenGod identity

Use icons/opengod.svg for the shell, starting page, footer and favicon. Its open-ring G and star sit on an opaque dark tile, remaining legible in both themes. The Windows icon includes 16 through 256 pixel sizes. Rebuild it with tools/build-icon.ps1 after geometry changes. The native DLL supplies the window icon; tools/set-package-icon.ps1 brands the packaged executable. The obsolete screenshot was removed from the README.
