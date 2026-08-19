# Environment: OpenCodeBox

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
