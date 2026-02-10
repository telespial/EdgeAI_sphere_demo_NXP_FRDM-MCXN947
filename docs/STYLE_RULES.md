# Style Rules (Text, Docs, Comments)

## Current Status Pointer
For current build behavior and restore pointers, see `STATUS.md` and `docs/RESTORE_POINTS.md`.

Goals:
- Keep documentation and comments objective and repo-centric.
- Avoid conversational phrasing that depends on a speaker/listener.

Rules:
- Do not use first-person or second-person wording in comments or docs (avoid words such as `I`, `me`, `my`, `we`, `our`, `you`, `your`).
- Do not reference chat agents or tooling personas in repo text (avoid `ChatGPT`, `Codex`, `assistant`, `AI model`, `language model`).
- Prefer neutral imperative or descriptive wording.
  - Good: "Set `WS_DIR` for a non-default workspace."
  - Avoid: "If you use a non-default workspace, set `WS_DIR`."

Enforcement:
- Run `./tools/lint_text_style.sh` before committing changes that touch docs or comments.
- Optionally install the git pre-commit hook via `./tools/install_git_hooks.sh`.
