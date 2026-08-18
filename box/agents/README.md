# box/agents — the layered agent brain

The main `AGENTS.md` (box root) is **generated** from these layers by
`box/scripts/tools/compose-agents.sh`. Do not edit the generated `AGENTS.md`
by hand — edit the layers, then re-compose (every box launch re-composes
automatically, and every button toggle does too).

## Layer order

Composed in this exact order (first wins on conflicting topics):

| Dir            | What it holds                                    | Button        |
|----------------|--------------------------------------------------|---------------|
| `0modes`       | Agent mode behaviours (build/plan/chat/…)        | `/modes`      |
| `1logics`      | Logic / reasoning-style instructions             | `/logics`     |
| `2knowledges`  | Knowledge: Linux, opencode2 beta, box structure, boundaries | `/knowledges` |
| `3instructions`| Workflow instructions (memory→history→backups→git, commits, handoffs) | `/instructions` |
| `4characters`  | Persona / tone                                   | `/characters` |
| `5commands`    | Command catalogue + button docs                  | `/commands`   |
| `deepthinking` | Deep thinking ON/OFF (OFF = token/money saving)  | `/deepthinking` |
| `reasoning`    | Reasoning effort level                           | `/reasoning`  |
| `contextlimit` | Per-chat context limit (66k→1M, step 66k)        | `/contextlimit` |
| `thinklimit`   | Thinking window limit (0→666k, step 33k)         | `/thinklimit` |
| `memory`       | Memory collection/usage flags                    | `/memorycollect`, `/memoryuse` |
| `classic`      | The classic monolithic AGENTS.md (fallback)      | `/agentsmodes` |

## Option system

Each layer contains **option directories** named `N-name` (e.g. `1-on`, `2-explorer`):

- `0-empty` = the EMPTY option (button off). **If it exists, it is the default.**
- `1-*` = the default **when no `0-empty` exists**.
- `2-*`, `3-*`, … = unlimited additional options.
- Each option dir contains one or more `*.md` files, concatenated in filename
  order when that option is active.
- `ACTIVE` (a file inside the layer) stores the currently selected option dir.
  It persists on disk → every new/restarted session starts with the same
  button state the previous session had.

## Special layers

- `deepthinking/` — options `0-off`, `1-on` (ACTIVE defaults to `1-on`).
- `reasoning/` — options `1-low`, `2-medium`, `3-high`, `4-ultra`
  (ACTIVE defaults to `2-medium`).
- `contextlimit/` — no option dirs; `VALUE` holds the limit in tokens
  (default `330k`), slider range 66k→1M step 66k.
- `thinklimit/` — `ON` (`0`/`1`, default `0`) and `VALUE` (default `99k`),
  slider range 0→666k step 33k.
- `memory/` — flags `.collect` and `.use` (`0`/`1`), content dirs `0-off/`, `1-on/`.
- `classic/AGENTS.classic.md` — the classic monolithic AGENTS.md used when the
  layered override is OFF (see `box/agents/.override`, toggled by `/agentsmodes`).
- `MOTIVATION.md` — **reserved for the box owner.** Write your motivational
  speech here; it is always placed at the very top of the composed AGENTS.md.
- `.override` — `1` = use the layered system (default), `0` = use classic.

## Manage buttons

Use the slash commands (`/modes`, `/logics`, …) or
`box/scripts/tools/toggles.sh` directly:

```sh
box/scripts/tools/toggles.sh list characters      # list options
box/scripts/tools/toggles.sh set characters 2-explorer
box/scripts/tools/toggles.sh cycle characters     # next option
box/scripts/tools/toggles.sh set deepthinking on  # on|off
box/scripts/tools/toggles.sh set contextlimit 330k
box/scripts/tools/toggles.sh set thinklimit up    # up|down|<n>k
```

State files are per-box (this box) and per-project (each project has its own
`box/agents/…`), so every project has independent button settings.
