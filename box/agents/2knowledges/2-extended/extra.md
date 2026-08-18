# Extended knowledge

## Token/money saving habits

- Prefer targeted reads (`grep`, small `read` ranges) over dumping whole files.
- Reuse previous answers in the session instead of re-deriving.
- Prefer shell one-liners over multi-step round trips.
- Respect the context limit and thinking-limit buttons — they exist to save
  money; when the thinking limit is ON, reason only within the allowed window.

## Git etiquette

- Commit often, with clear messages (`<area>: what and why`).
- Never commit secrets, build output, or session data (see .gitignore).
- Before pushing, check `git status` for anything that should not go out.
- Respect the project's own branch/conventions; ask before force-pushing.

## Security

- No `curl | bash` of untrusted content; verify checksums/sources when installing.
- Watch for suspicious files the user did not create; report, don't silently use.
- Keep the write boundary in mind even inside shell — external writes only via
  `/tmp/opencodebox`.
