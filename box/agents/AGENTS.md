# OpenCodeBox — temporary factory reset

This folder is a temporary **factory-new opencode2** setup:
- auto-compaction **ON**
- no box rules, no shell whitelist, no custom agents/skills/commands
- providers/models come from the global opencode config

The full OpenCodeBox (scripts, plugins, skills, agents, data, git repo) is
backed up at `/run/media/hubertg/SONIC/opencodeboxold/`.

**Restore:** move everything from `opencodeboxold/` back here (projects/ stayed
in place) and delete this AGENTS.md + opencode.json (originals are in the backup).
