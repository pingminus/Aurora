# Contributing

Read the root engineering constitution and the relevant specialist role before changing a boundary. Start from a reproducible failure or concrete user behavior; keep changes small enough to review and do not add unused module skeletons.

For a feature, define the state invariant and command, implement native policy/effects, connect the TypeScript view, add meaningful regression coverage and update the corresponding documentation. Preserve unrelated work. For a CEF change, include the SDK revision, resource/subprocess implications and sandbox verification.

Use `.editorconfig` and `.clang-format`, strict TypeScript and existing CSS tokens. Run the applicable checks from `docs/BUILD.md` and `docs/TESTING.md`. Describe what was tested and what remains unverified. Do not replace failing behavior with fake responses, ignore compiler warnings or hide unsupported features behind working-looking controls.

Use focused commits such as `feat(tabs): preserve selection when closing a background tab` or `fix(ipc): reject untrusted subframe commands`. Keep generated files, dependency archives, local profiles and logs out of commits. Review dependency licenses and record substantive architectural choices in `docs/DECISIONS.md`.
