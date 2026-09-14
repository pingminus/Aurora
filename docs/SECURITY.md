# Security model

## Trust boundaries

Websites, navigation input, renderer messages and downloaded bytes are untrusted. The TypeScript shell is packaged application code but its commands must still be validated. The browser-process controller owns policy. CEF supplies Chromium's sandbox, certificate verification and web-origin enforcement; OPENGOD must not disable them.

Only the dedicated shell browser's main frame at the exact internal origin may submit privileged commands. Check all three properties before parsing. Do not grant access based only on a URL prefix, browser process membership or a TypeScript type. Reject malformed JSON, unsupported methods, invalid identifiers, excessive input and unexpected schemes without mutation. Escape serialized state and render text through safe DOM APIs.

Keep the internal resource handler restricted to packaged paths with a strict MIME map and content security policy. Reject traversal and unknown resources. Never route arbitrary filesystem paths or network requests through the privileged origin. Web tabs may not navigate the trusted shell, open privileged popups or acquire shell capabilities by loading an internal URL.

## Platform controls

Windows CEF sandbox support is mandatory. Current CEF bootstrap requirements must be followed, including the sandbox-compatible entry point and subprocess lifecycle. Do not ship `--no-sandbox`, certificate-ignore, remote-debugging or web-security-disable flags. Unsupported external protocols, downloads and permissions are denied until an explicit implementation exists. Certificate failures should remain errors, never silent successes.

Profile isolation must cover CEF request contexts and application storage together. Future persisted grants are origin-bound, revocable and profile-specific. Guest/private storage requires an explicit ephemeral lifecycle; a differently named tab or directory is not proof of isolation.

The website-session preference is off by default and saved in the current user's registry hive. Off uses CEF's in-memory default profile, so profile-specific website data is not saved to disk. On uses the persistent default profile under the current Windows user's local application data and retains session cookies as well as longer-lived website data. A change takes effect after restarting OpenGod. Turning it off does not delete data already saved in the persistent profile, and CEF may retain installation-specific data under its root cache path. Signing out on a site or clearing its cookies remains the way to end that site's session. OpenGod does not read or log cookie values.

## Release evidence

Before distribution, verify bridge rejection from hostile pages and subframes, traversal rejection, external-scheme blocking, permission denial, certificate errors, sandboxed subprocesses and clean shutdown. Record the CEF/Chromium revision, dependency notices and upgrade procedure. Test renderer crashes and corrupted local state. Sanitization is defense in depth; browser/frame authorization remains necessary.

Production logs must exclude visited URLs, search strings, cookies, credentials and page text by default. Diagnostics should report aggregate counters and build details, with explicit opt-in for sensitive debugging. There is no claim of a completed security audit or production readiness.
