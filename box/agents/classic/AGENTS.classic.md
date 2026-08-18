# OpenCodeBox — Closed Sandbox Environment

## opencode2 (V2) — this box runs V2
- The box launches the **opencode2** binary (V2 beta) as `opencode2 --standalone --auto [--continue]`, with its own XDG data dir (`.opencode-data/`), agents/commands/skills symlinked from the box, and V2-config files (opencode.json uses the ordered `permissions` array; agents → `agents`, skills → `skills` list).
- Helpful V2 commands: `opencode2 service status|restart`, `opencode2 api get /api/health`, `opencode2 export --standalone -s <id>`, `opencode2 import --standalone <file>`, `opencode2 run "..."`, `opencode2 mini`.
- V2 keeps V1 session history in the same DB (`session_v2`); live context = `tokens.input + tokens.cache.read` of the latest assistant message (`session_message`).
- The V2 TUI has no tab bar in this beta yet — the limit formula is a **clean relay**: handoff + memory, launch a fresh continuation terminal, then gracefully close the old chat (see `scripts/auto-handoff.sh`).
- GitHub MCP tools are auto-approved via the `github_*` permission rule.

This is a **closed, restricted environment** hosting the opencode setup.
Launch it with `scripts/opencodebox.sh` (opens a new terminal window); all config lives
in this `OpenCodeBox/` folder. The whole setup is **directory-agnostic**: it
adapts to whatever folder contains it, re-scoping edits to that container.

## This box at a glance — know this first

- **The box is a hub.** `OpenCodeBox/` hosts all agents, commands, skills, and
  plugins. Every project on the drive (`/run/media/hubertg/SONIC/OpenCodeBox/projects/<Project>/`)
  is served by the same system: each project's `.opencode/skills`,
  `.opencode/command`, and `.opencode/agent` are **symlinks to the box's**, so
  project terminals get the exact same modes and tooling as the box.
- **Plugins** live in `OpenCodeBox/plugins/<Name>/` — each plugin is its own
  project folder with an `addon/` subfolder for addons that extend it. Installed
  plugins: `memory` (per-project memory), `git` (commit/push), `CodePreferences`
  (preferred stack), `providers` (models/plan), `LocalWaste` (dump instead of
  delete), `handoff` (session continuity), `srcbackups` (source snapshots),
  `opensidebar` (TUI sidebar). The plugin's real source of truth may be its own
  project inside `projects/` (e.g. `projects/CodePreferences/`,
  `projects/autocommit/`, `projects/nonstop/`, `projects/opensidebar/`,
  `projects/LocalWaste/`, `projects/providers/`); the `plugins/<Name>/` folder
  is the wiring/registration.
- **Skills** live in `.opencode/skills/<name>/SKILL.md`; **commands** in
  `.opencode/command/<name>.md`; **agents** in `.opencode/agent/`.
- **Sessions/handoffs/memory** are per-project under `data/projects-data/<Project>/`
  and the memory plugin (`plugins/memory/memory/`); details below.
- **Code preferences** are defined by the CodePreferences plugin — read
  `CodePreferences/preferences.conf` and follow its ordering before choosing
  languages / libraries / systems (see the "Code preferences" section below).

## Auto-continue preference (user's operating mode)

- If opencode appears to be waiting on a process/timeout (e.g. a shell command
  that takes time, a build, a long tool call), wait about **6 seconds** and then
  **continue automatically** as if the user typed "continue".
- Do not repeatedly ask the user to confirm continuation during long-running
  tasks — proceed on your own after the short wait.

## User-made changes — ask before touching

- If you notice changes in the project files that no agent in this box made
  (the user edited them directly), **ask the user about them first** — what
  they changed and what they want done with it — before modifying, reverting,
  or building on top of them.
- Never silently overwrite, undo, or "fix" user-made edits. Treat them as
  intentional until the user says otherwise.

## Useful commands — show these at the start of every new chat

At the beginning of each new conversation (right after the first user message),
briefly show these handy opencode key commands so the user always has them:

```
Ctrl+X  B   sidebar (agents / tools / files)  ← default open
Ctrl+X  L   sessions list  (switch/resume sessions)
Ctrl+X  A   agents list    (switch agent)
Ctrl+X  N   new session
Ctrl+X  M   models         (pick a model)
Ctrl+X  S   status view
Ctrl+X  E   open editor
Ctrl+X  T   themes
Ctrl+X  H   toggle thinking blocks
Ctrl+P      command palette
Shift+Tab  cycle agents
```
Keep it short (one compact block) — do not repeat it on every message, only at
the start of a new session.

## Your workflow — the four core skills (auto-load every chat)

This box runs a **grill → decide → handoff** workflow. At the start of every
new chat, load these four skills (invoke each skill once, before doing any
work):

1. **`grilling`** — the interview primitive: grill the user relentlessly in
   rounds until a shared understanding is reached. Before acting on any
   non-trivial task, grill first. Facts are yours to find; decisions are the
   user's.
2. **`ponytail`** — lazy-senior-dev mode: force the laziest solution that
   actually works (YAGNI, stdlib first, shortest diff). Apply to every
   implementation.
3. **`wayfinder`** — for efforts too big for one session: chart a shared map
   of decision tickets and resolve them one per session until the way is
   clear. Reach for it when the user hands you a huge, foggy goal.
4. **`ask-matt`** — the router: if you or the user are unsure which skill or
   path fits, consult it.

**Grill before you build.** For any non-trivial task, run `grilling` first
and get the user to confirm a shared understanding before writing code. Only
skip it for trivial one-liners. `wayfinder` is for what grilling can't hold;
`ponytail` rides on every build.

## Agent modes (Shift+Tab / Ctrl+X A to switch)

Custom primary agents live in `.opencode/agent/` (symlinked into every
project). They all behave conversationally — short, interruptible turns — and
only work on the project when permitted:

- **`chat`** — pure casual chat. Does no project work and never plans. Tools
  are `ask`-gated: usable only when the user explicitly agrees or asks.
- **`vision`** — chat-like but vision-curious. Read-only + question; adds the
  main points the user raises to the ongoing plan as high-level headlines.
- **`detail`** — a precise interviewer. Read-only + question; asks focused
  questions and records the specifics into the ongoing plan.
- **`adminchat`** — casual chat with **full build-mode environment access**.
  Builds ONLY when the user explicitly agrees to build or commands it; never
  plans or edits uninvited.
- **`notes`** — quiet listener. Never talks back; captures useful info the
  user mentions into `data/notes/<topic>.txt` per project (append-only,
  timestamped). No building, no planning.
- **`agent`** — fully automated worker. Runs ongoing looped tasks with the
  full build toolset, asks what to do whenever the user starts talking, and
  logs every action to `data/notes/<YYYYMMDD>.txt` per project.
- **`describe`** — read-only project explainer. Ignores the user's literal
  message and describes the current project in plain, human-readable language
  (headlines, short chapters). No building, no planning.

As `build`/`plan`, the custom modes never modify anything outside the box
boundary (the whitelist applies to every agent).

## Context guard — auto-handoff at the context limit

Every launcher spawns a background **watcher** (`OpenCodeBox/scripts/auto-handoff.sh
--watch`) that polls this instance's session and, when context usage crosses
the limit, writes a handoff, opens a fresh session that continues from it, and
closes the old one. Limits (env-overridable): `CONTEXT_LIMIT` (default 303696,
the measured ceiling for `deepseek-v4-flash-free`), soft at 85% (~258k, clean
handoff), hard at 100% (interrupt + handoff). Auto-compaction is disabled
(`OPENCODE_DISABLE_AUTOCOMPACT=1`) so handoff is the only compaction path.

- **Never push on degraded context — hand off at the boundary.**
- When you sense you're near the limit, or a phase boundary is reached, write
  a **`/handoff`** (the `handoff` skill) into the per-project handoffs folder
  so the watcher (or `--trigger`) picks up a rich doc instead of a
  machine-generated one.
- **If you hit a context-overflow error, or the watcher interrupts you
  (SIGINT/SIGTERM): stop, write a `/handoff` immediately, then run
  `bash OpenCodeBox/scripts/auto-handoff.sh --trigger --force --name <box|Project> --data-dir <instance .opencode-data> --handoffs <instance handoffs> --pidfile <instance opencode.pid> --workdir <project dir> --restart-cmd <launcher>`.**
  You can inspect usage anytime with `--check` (same args).
- **At the start of every session**, check for a `LATEST.md` handoff doc
  (`OpenCodeBox/data/projects-data/<Project>/handoffs/LATEST.md`, or
  `OpenCodeBox/data/handoffs/LATEST.md` for the box itself). If one exists and the
  user's message is about the same work, load it and continue from its "Next
  steps" instead of re-asking settled questions.
- The `context-guard` skill encodes all of this; follow it automatically.

Slash commands available: `/grill <topic>`, `/wayfinder <idea>`,
`/ponytail [lite|full|ultra]`, `/handoff [focus]`, `/memory`,
`/backup`, `/git`, `/gitall`, `/repos`, `/limits`, `/force`,
`/profilerefresh`, `/publicall`, `/privateall`, `/publicrepo`,
`/privaterepo`, `/continue`.

## Per-project memory (the memory plugin)

All project memory lives in the **memory plugin** (a plugin like any other, with
its own project layout), all gitignored, never committed:

```
OpenCodeBox/plugins/memory/memory/
  longmemory/   <Project>-longmemory.txt   STRUCTURE ONLY: directory tree,
                                           file names + extensions + sizes
  shortmemory/  <Project>-<stamp>-shortmemory.txt  KEYWORDS + CORE FUNCTIONS +
                                           recent messages/commits
  filememory/   <Project>-filememory.txt   EXACT LOCALIZATION (file:line index)
  oldmemories/  archive (never delete, always archive)
```

- **longmemory** stores ONLY the structure: directories, file names/extensions,
  file sizes. Nothing deeper.
- **shortmemory** stores keywords of files and core functions — of the whole
  program and of each function — plus recent messages and commits.
- **filememory** pins exact locations: which file holds which function/class,
  with `file:line`.

**Lookup order when you need project info**: first **longmemory** (is the topic
even mentioned? what files exist?), then **shortmemory** (keywords, core
functions), then **filememory** (exact file:line). If anything is still unclear
or wrong, **always 2nd-check the exact location** of the information directly in
the real source file — filememory is an index, not the truth.

Limits: `shortmemory/` 256 KB total, `longmemory/` 64 KB (env-overridable
`SHORT_LIMIT` / `LONG_LIMIT`). Oldest captures move to `oldmemories/` automatically.

The capture is driven by **`/memory`** (a command available to every agent) and
by **`OpenCodeBox/plugins/memory/memory.sh`** (`--workdir DIR [--data-dir DIR]
[--name NAME] [--notes "..."]`; data-dir auto-detects from the standard box
layout). The auto-handoff watcher also runs it right after writing a handoff,
before the fresh terminal restarts.

Agents should run `/memory` before `/handoff` and at phase boundaries.

The **`memory` skill** (`.opencode/skills/memory/SKILL.md`) is the custom
instruction for this: load it to read existing memory at session start (long →
short → file, in that order), capture after significant work, and archive
memories that are no longer useful. **`/oldmemories`** (or `/oldmemories
list|prune`) manages the archive — move superseded memories to `oldmemories/`
instead of keeping them in the active layers (never delete; if unsure, keep).

## Token / limit tracking (session + daily)

The box tracks token/context usage against both the **per-session** context
limit (soft ~85% / hard 100% of the model context) and a **daily** token
limit. The `auto-handoff.sh` watcher records usage each poll into
`OpenCodeBox/data/limitlogs/`, with informative filenames:

- `sessions/<stamp>_<session>_<soft|hard>_<used>_<limit>.txt` — per-session
  soft/hard hit (latest + history kept).
- `daily/<YYYY-MM-DD>_daily_used_<used>_of_<limit>.txt` — running daily total.

State flags live in `OpenCodeBox/data/`:
- `.careful_mode` — set when the daily **soft** limit is reached: the agent
  should treat this as the last session, save often (`/backup` / `/gitall`
  after each task), and prepare a `/handoff`.
- `.force_daily` — set by **`/force`** to permit crossing the daily **hard**
  limit once (overriding the block).


Check status anytime with **`/limits`** (`bash OpenCodeBox/scripts/limits.sh --status`).
Storage is especially useful right after a context-limit restart: read
`data/limitlogs/` to see exactly when/at what usage the session or day reset.

## Code preferences (the CodePreferences plugin)

Before choosing a language, library, or target system for any code you write,
check the **CodePreferences** plugin:

- **Config**: `/run/media/hubertg/SONIC/OpenCodeBox/projects/CodePreferences/preferences.conf`
  (the source of truth; edit it to change the stack).
- **Resolver**: `bash /run/media/hubertg/SONIC/OpenCodeBox/projects/CodePreferences/src/codepref.sh`
  prints the effective settings; `codepref.sh <item>` checks a language/library.

Rules:
- **Order matters.** Use the first available option in each preferred list
  (`preferred_languages`, `preferred_libraries`, `main_system` + `additional_systems`),
  then fall back down the list in order. If none fit, use whatever is still allowed.
- **Never use blacklisted items** (`blacklisted_languages`, `blacklisted_libraries`).
- **Whitelist-only mode**: when `only_whitelisted_languages` /
  `only_whitelisted_libraries` is `true`, only the whitelisted items may be used.
- **Defaults for this box**: Linux, C++ (version: C++20 per
  `language_versions` in the plugin config), Vulkan. Prefer these unless the
  task requires something else that is not blacklisted.

## Your workspace / edit boundary

- You may **read** anywhere on this computer (all drives / home directory).
- You may **edit, write, create, or delete** files **ONLY** inside the box's
  own folder: `OpenCodeBox/` (the folder containing this box — that includes
  `projects/`, `data/projects-data/`, `plugins/`, `scripts/`, `tools/` (incl. `tools/src/`), `helpers/` (incl. `helpers/src/`), `dump/`, `waste/`).
- You may **NEVER** edit, write, or modify any file or directory outside the
  `OpenCodeBox/` folder — this includes the rest of the drive it sits on,
  `/home`, `/run`, `/opt`, `/etc`, other mounted drives, or anywhere else.
- Treat everything outside the `OpenCodeBox/` folder as **read-only**.

## Shell (bash) usage

- Shell commands are **heavily restricted** by an explicit whitelist in
  `opencode.json`. Any command not whitelisted is denied automatically.
- **Read-only** shell commands (`cat`, `ls`, `find`, `grep`, `rg`, `df`, `du`,
  `ps`, `free`, `mount`, `file`, `stat`, `git status/log/diff`, ...) are allowed
  **anywhere**, including outside this drive, so you can inspect the whole PC.
- File-modifying shell commands (`rm`, `mv`, `cp`, `mkdir`, `touch`, `chmod`,
  `git`, build tools, ...) must **explicitly reference** the `OpenCodeBox/`
  folder in their arguments.
- For toolchain / project commands, always keep the working directory inside
  the `OpenCodeBox/` folder (e.g. `cd OpenCodeBox/projects/<project>` first),
  or use the supported path flags (`--prefix`, `-C`, `--cwd`,
  `--manifest-path`, ...).
- **Note:** the `edit`/`write` tools are blocked on nested paths under this
  sandbox; use `tee` heredocs (whitelisted as `tee /run/media/hubertg/SONIC/OpenCodeBox*`)
  to create or modify files.

## Internet

- You **may** browse the internet (`webfetch`, `websearch`) freely.

## Self-contained environment

This environment is self-contained inside the `OpenCodeBox/` project folder on
the drive. Its own configuration and tooling live here and are loaded when
launched via `scripts/opencodebox.sh`.

- **Launcher**: `scripts/opencodebox.sh` opens a new terminal window. By default it
  CONTINUES THE LAST SESSION (`--continue`); pass `new` for a fresh session or
  `continue` for the auto-handoff continuation prompt.

## Box layout (uniform structure)

```
OpenCodeBox/
  opencode.json         generated box config (permissions + providers merge)
  AGENTS.md / README.md docs
  scripts/             ALL .sh scripts live here (incl. launcher opencodebox.sh,
                        auto-handoff, box-env,
                        make-project-runs, srcbackup, limits, ...)
  plugins/<Name>/       plugins: memory, git, CodePreferences, providers,
                        LocalWaste, handoff, srcbackups, opensidebar
                        (each with addon/ wiring; real projects may live in
                        projects/<Name>/)
  projects/             ALL projects live here now (each keeps its own git
                        repo; gitignored by the box)
  data/                handoffs, sessions, limitlogs + projects-data/
  skills/              custom box-level skills (committed; wired into opencode.json)
  tools/               always-used compiled tools + box tool wiring
                        (openbox-keys, ProjectTXT)
  tools/src/           C++20 sources of the always-used tools
  tools/output/        output the tools produce during their work
                        (gitignored, manual cleanup only)
  helpers/             on-demand helper tools, used when needed or on
                        command (final-verify)
  helpers/src/         C++20 sources of the helper tools
  helpers/output/      output the helpers produce during their work
                        (gitignored, manual cleanup only)
  dump/                temporary helper files opencode creates during work
                        (gitignored, manual cleanup only)
  dump/output/         output produced during dump/ work
                        (gitignored, manual cleanup only)
  waste/                LocalWaste dump (manual cleanup only)
  TokenKeys.cfg         API keys/tokens template (gitignored; "PASTE API/TOKEN
                        HERE" placeholders)
```
**Layout rules (uniform across the box and every project)**: `.sh` scripts →
`scripts/`, C++20 code → `tools/src/`, compiled executables + tools like
ProjectTXT → `tools/`, plugins + addons → `plugins/`, deleted-file dumps →
`waste/`.

**The three helper spots — where a helper file goes (pick by usage):**
- **`tools/` (+ `tools/src/`)** — helper tools that are ALWAYS used (e.g.
  `openbox-keys`): the compiled binary lives in `tools/`, its C++20 source in
  `tools/src/`. Per-project `tools/` also holds the provisioned ProjectTXT.
- **`helpers/` (+ `helpers/src/`)** — helper tools that are NOT always used
  but can be used when needed or on command (e.g. `final-verify`): binary in
  `helpers/`, source in `helpers/src/`.
- **`dump/`** — ALWAYS dump every temporary/scratch helper file opencode
  creates during work here (analysis scripts, API dumps, debug snippets,
  notes). Gitignored; only a MANUAL cleanup may empty it.
- **`output/`** — each of `tools/`, `helpers/`, `dump/` has its own
  `output/` subfolder: anything a tool/helper produces during its work
  (reports, generated files, results) goes there, never anywhere else.
  Gitignored; only a MANUAL cleanup may empty it.
- **`waste/`** — files that get deleted go to LocalWaste, never into `dump/`
  and never `rm` for good.

## Providers (the providers plugin)

The box's model providers are configured by the **providers plugin**
(`plugins/providers/providers.conf`) + `TokenKeys.cfg`:

- `opencode-go` carries the BEST models (deepseek-v4-pro, deepseek-v4-flash,
  kimi-k3, glm-5.3, qwen3.8-max, hy3, mimo-v2.5-pro, minimax-m3, gpt-5.6-luna).
- `opencode-zen` carries PAID models only.
- `opencode-zen-free` carries FREE models only (non-free models removed).
- Default model follows `PLAN` in providers.conf: go → deepseek-v4-pro,
  zen → deepseek-v4-flash-free.
- API keys/tokens come ONLY from the gitignored `TokenKeys.cfg`; blank
  entries are skipped and the global opencode config is the fallback.
- `plugins/plugins.conf` is the central settings file other plugins read
  (handoff limits, memory limits); environment variables still win.

Old structure (everything at the drive root) has been migrated:
ALL projects now live under `OpenCodeBox/projects/` and the box is the hub.

- **Chat history / data**: `.opencode-data/` — own session DB, not the global one.
- **Sessions**: `data/sessions/` — visible session backups (JSON), auto-exported on
  exit by `scripts/opencodebox.sh`. Restore with `session-restore.sh`, export with
  `session-backup.sh`.
- **Handoffs**: `data/handoffs/` (box-level) and `data/projects-data/<Project>/handoffs/`
  (per-project) — handoff documents written by `/handoff` for session-to-session
  auto-continue. Never commit these.
- **Skills**: `.opencode/skills/` (registered under `skills.paths`).
- **Agents**: `.opencode/agent/`.
- **Commands**: `.opencode/command/`.
- **Plugins**: `plugins/` — each plugin is a separate project folder (e.g.
  `plugins/memory/`), with its own `addon/` subfolder for that plugin's addons.
- **Scripts**: `scripts/` — shared helper scripts for the box.
- **Plugins (opencode)**: `.opencode/plugin/` — opencode's own plugin loader dir.

Work on the projects at
`/run/media/hubertg/SONIC/OpenCodeBox/projects/<Project>/`.

## Skills & commands structure (how this box works)
 - Custom skills: `skills/` at the box root (box-wide) and `<Project>/skills/`
    (per-project) — both registered in `skills.paths`, committed, never symlinked.

- **Skills** live in `.opencode/skills/<name>/SKILL.md`. Each `SKILL.md` has
  frontmatter (`name`, `description`) + instructions. Installed skills:
  - Workflow core (auto-load each chat): `grilling`, `ponytail`, `wayfinder`,
    `ask-matt`.
  - Session continuity: `handoff`, `context-guard`, `session-history`.
  - Box helpers: `sonic-project-doc`, `projecttxt-dump`, `github-autocommit`.
- **Commands** live in `.opencode/command/<name>.md` with frontmatter
  (`description`, `agent`) + a prompt body. Commands: `projects`, `status`,
  `project-dump`, `grill`, `wayfinder`, `ponytail`, `handoff`,
  `session-history`, `memory`, `backup`, `git`, `gitall`, `repos`,
  `limits`, `force`, `profilerefresh`, `publicall`, `privateall`,
  `publicrepo`, `privaterepo`, `continue`.
- The same skills & commands are **symlinked into every project** at
  `.opencode/skills` / `.opencode/command`, so EVERY project terminal can use
  them. They are never committed to the project repos.

## One system: the box is the hub, projects share it

The **OpenCodeBox terminal is the main agent hub** — the one place where all
agent modes (chat, vision, detail, adminchat, notes, agent, describe), commands, and
skills live. Per-project terminals are **the same system**, not a separate
setup: every project's `.opencode/skills`, `.opencode/command`, and
`.opencode/agent` are **symlinks to the box's**, so project agents get the
exact same modes and tooling as the box itself.

- The box = the hub. Project terminals = the same box scoped to one folder.
- A project's `.desktop` file (if any) is for **launching the project's own
  built app** (game/bot/...), never for starting an opencode terminal. To talk
  to an agent about a project, use the box, or the project's run file — not a
  `.desktop` that opens opencode.
- **Only one agent per project (or per box) can run at a time.** Launching a
  second terminal for a project/box that is already running is refused with a
  clear message (the single-instance guard). The only way a terminal is
  replaced is the auto-handoff continuation restart. This keeps the SQLite
  session DBs from fighting over the same files.

### Per-project run files & sessions

Each project on the drive has its **own run file** and its **own isolated
sessions**:

- Run file (launch from the box): `<Project>/run-<Project>.sh`
- Run file (launch from inside the project): `<Project>/run-<Project>.sh`
- Config: `data/projects-data/<Project>/opencode.json`
- Live session DB: `data/projects-data/<Project>/.opencode-data`
- Session backups: `data/projects-data/<Project>/sessions/` (auto-exported on exit)
- Handoffs: `data/projects-data/<Project>/handoffs/` (session-to-session continue)

These are generated by `make-project-runs.sh` (re-run it when you add a new
project folder). Each project run only edits inside its **own** project folder
and keeps its conversations/sessions fully separate from every other project.

Every project terminal:
- Opens in a **new, visible terminal window** that stays open and is fully
  interactive (you can type in it) — spawned by `box-env.sh`'s
  `spawn_terminal` (detects alacritty / konsole / gnome-terminal / kitty, ...).
- Uses the box's shared skills & commands (symlinked into `.opencode/`).
- Uses the **ProjectTXT** tool (see below) to read the whole project at once
  whenever a full read is needed.
- Is **single-instance**: a second launch while it is running is refused with
  "already running (PID ...). Only one agent per project."

## How to find project information (search order)

When you need information about a project, search for it **in this order** —
use the first source that answers the question, and only move to the next if
it doesn't:

1. **Current chat** — what the user is saying/asking right now.
2. **Current session history** — this opencode session's own messages.
3. **Project memory** — check the memory plugin (longmemory first — structure,
   then shortmemory — keywords/core functions, then filememory — exact file:line)
   for anything the user told you that survives across sessions.
4. **Per-project sessions** — earlier session backups for this project at
   `OpenCodeBox/data/projects-data/<Project>/sessions/*.json` (auto-exported on exit).
5. **`tools/ProjectTXT.txt`** — the whole-project dump. Produce it first by
   running `tools/ProjectTXT` (see the ProjectTXT section below), then read
   the generated `tools/ProjectTXT.txt`.

## Committing work — use the commands, not ad-hoc git

Every project folder under `/run/media/hubertg/SONIC/` is a git repository with
its own GitHub remote (repo name = folder name under `Gyerchak/`). Instead of
manually running `git add`/`commit`/`push` after every change, use the box's
commit commands. The default workflow at the end of any working session is
**`/backup` then `/gitall`**:

- **`/git <Project...> "<message>"`** — commit (and push) the current project,
  or one or more explicitly named projects, with a diff-based message.
- **`/gitall`** — run `/backup` first, then commit + push every repo under the
  container (including OpenCodeBox), each with its own message from its diff.
- **`/backup`** — snapshot each project's git-tracked source into
  `OpenCodeBox/srcbackups/` (`latest/` + dated `archive/`).
- **Recall older versions (the backups plugin)** — `srcbackups/` is a per-project
  source snapshot store you can use to recall an older state of a project:
  - `bash OpenCodeBox/scripts/srcbackup.sh --list` — see stored snapshots per project.
  - `OpenCodeBox/srcbackups/latest/<Project>/` — newest snapshot of a project.
  - `OpenCodeBox/srcbackups/archive/<Project>/<YYYYMMDD-HHMMSS>/` — dated history.
  - When a project is broken, work is lost, or you need to see an older version,
    recall it from a snapshot instead of guessing: copy the file(s) you need out
    of `latest/<Project>/` (or a dated archive snapshot) back into the project.

The commands run a **noise check** automatically (relying on `.gitignore`):
never commit build artifacts, secrets, `tools/`, `plugins/`,
`node_modules/`, `.opencode-data/`, `sessions/`, memory dirs (`shortmemory/`,
`longmemory/`, `oldmemories/`), `data/`, or `srcbackups/`. If any such file
shows up in `git status`, untrack it (`git rm -r --cached <path>`) and add a
`.gitignore` entry — never leave noise in `git status`.

Rules:
- Commit **after every meaningful change** — don't leave work uncommitted at
  the end of a session. Use `/git` or `/gitall` rather than raw git commands.
- The commit message must be auto-written from the actual diff (summarize what
  genuinely changed).
- **Every repo's `.gitignore` must include `tools/` and `plugins/`** — the
  `tools/` dump contains the whole project (source + structure) and may hold
  sensitive info. If a repo's `.gitignore` lacks them, add them.
- If a project is **not** yet a git repo, initialize it via the repo commands
  (`/publicall`, `/privateall`, `/publicrepo`, `/privaterepo`, `/repos`) rather
  than by hand. These scaffold the default dir structure and generate a
  dynamic LICENSE/README (owner + date).
- If pushing fails because GitHub is not authenticated, stop and tell the user
  to run `gh auth login`.

## You (OpenCodeBox) are a GitHub repo

- You, OpenCodeBox, are themselves a GitHub repository:
  - Remote: `https://github.com/Gyerchak/OpenCodeBox.git`
  - Repo: `Gyerchak/OpenCodeBox` (PUBLIC), default branch `main`
- This means you are a git repo too: `git status` / `git -C /run/media/hubertg/SONIC/OpenCodeBox ...`
  work for your own files here.
- You also commit your own changes with the box commands: after you modify
  anything in this `OpenCodeBox/` folder (config, scripts, AGENTS.md, README),
  run `/git OpenCodeBox "<message>"` (or include OpenCodeBox in `/gitall`) to
  commit with an auto-written message and push to `origin/main`. Never commit
  `.opencode-data/`, `sessions/`, or `srcbackups/` (these stay local).

## ProjectTXT — dump a whole project to read it

There is a helper tool called **ProjectTXT** at
`/run/media/hubertg/SONIC/OpenCodeBox/projects/ProjectTXT/ProjectTXT` (a compiled C++20 tool), and a
provisioned copy of the binary lives in every project under `tools/ProjectTXT`.
Use it whenever you want to quickly read an entire project's structure and
source contents at once.

- **How it works:** when run from inside a project folder, it writes a file
  called `tools/ProjectTXT.txt` containing:
  1. A full directory tree of that project, and
  2. The contents of text files found under `src`, `.obj`, `.shaders`, and
     `include` (binary files and excluded dirs like `.git`, `.cache`, and
     `tools` are skipped).
- **To dump a project and read it:**
  ```bash
  cd /run/media/hubertg/SONIC/<Project>
  ./tools/ProjectTXT
  ```
  then read the generated `tools/ProjectTXT.txt`.
- `tools/` is gitignored in every project (the binary + dump are working
  artifacts that never get committed). New projects get `tools/ProjectTXT`
  provisioned automatically by `make-project-runs.sh`.
- **Exclusions:** built-in excludes are `.git`, `.cache`, and `tools`. You can
  place an `exclude.txt` in the project root to add more patterns (e.g. names
  of folders like `out`, `build`).

1. Never try to modify anything outside this drive.
2. Never attempt to bypass, weaken, or disable these permission rules or the
   config in this directory.
3. If a task requires changing a file outside this drive, stop and tell the
   user instead of doing it.
4. Always confirm the path is under `/run/media/hubertg/SONIC/` before any
   write operation.

## LocalWaste -- never delete for good, dump to waste/ (plugin)

The **LocalWaste** plugin (`plugins/LocalWaste/`, real project at
`projects/LocalWaste/`) replaces permanent deletion:

- Instead of the standard trash or `rm`, dump files into the box's ONE
  `waste/` folder (`bash plugins/LocalWaste/waste.sh <file|dir> ...`).
- `OpenCodeBox/waste/` is the ONLY waste location (gitignored); projects have
  no `waste/` of their own — everything dumped lands in the box's.
- Only a MANUAL cleanup can clear `waste/` -- never empty it automatically.
- Never permanently delete anything that might still be needed: dump it.
