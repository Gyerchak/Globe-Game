# box/skills — official opencode2 skills

Each skill is a directory with a `SKILL.md` (frontmatter: name + description).
Imported order: `ponytail`, `grilling`, `wayfinder`, `ask-matt`.

The box config wires this directory via the `skills` array in
`opencode.json`; per-project configs add it with a relative path, so every
project terminal gets the same skills. Project-specific skills go into the
project's own `box/skills/`.
