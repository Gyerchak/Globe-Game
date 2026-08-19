# Environment: my custom

> This option is YOURS — add every piece of information that does not fit
> any other layer here (context about your setup, people, workflows, etc.).
> It is applied on top of the basic OpenCodeBox environment.

## Basic system / PC info

- OS: Linux (CachyOS, rolling Arch-based; kernel 7.1.8-1-cachyos, x64)
- Desktop: KDE Plasma on X11 (konsole terminal)
- Shell: zsh (default), bash for scripts
- Tools available: git, gh CLI, cmake/make, g++ (C++20), python3 (3.14),
  sqlite3, jq, xprop — and opencode2 at ~/.opencode/bin/opencode2
- opencode2 version: 0.0.0-beta-17519 (beta channel)
- GPU/creativity: ComfyUI, Vulkan available (see preferences.md)
- Drive layout: projects live under /run/media/hubertg/SONIC/OpenCodeBox/

## The box environment

You are running INSIDE OpenCodeBox — a closed sandbox environment built
around opencode2 (V2 beta) on the user's Linux PC:

- **Closed boundary:** reads anywhere; writes ONLY inside the box container
  and /tmp/opencodebox (RAM scratch).
- **Your settings come from the box** — the composed AGENTS.md (box/agents
  layers) plus the button states the user set in the app.
- **Sessions are per-terminal** (own DB); the box is directory-agnostic and
  can be moved anywhere.
- **Deletion rule:** the only allowed delete method is moving things to
  box/waste/ — never rm.
- Secrets live in box/TokenKeysMCP.env (gitignored) — never print or commit them.

