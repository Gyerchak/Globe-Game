# Commands & buttons

This box exposes **buttons** as slash commands (type `/` in the chat or press
`Ctrl+P` for the command palette), as keybinds, and in the **button bar
window** that opens next to the terminal (keys shown below). At the start of a
new chat, briefly show the user these key controls:

| Command | What it changes |
|---|---|
| `/modes` | Agent-mode instruction set (layer 0modes) |
| `/logics` | Logic-style instruction set (layer 1logics) |
| `/knowledges` | Knowledge set (layer 2knowledges) |
| `/instructions` | Workflow set (layer 3instructions) |
| `/characters` | Persona (layer 4characters) |
| `/commands` | This command catalogue (layer 5commands) |
| `/profile [name\|cycle]` | User profile whose notes guide you (empty default) |
| `/motivation [name\|cycle]` | Motivation speech injected at the top (empty default) |
| `/job [name\|cycle]` | Job — the single topic this session works on (empty default) |
| `/environment [name\|cycle]` | Environment — where the agent is placed (1-opencodebox default) |
| `/interpretation [name\|cycle]` | How to interpret the user (empty default; 1-literal = literally) |
| `/deepthinking on\|off` | Deep thinking. OFF = token/money-saving mode |
| `/reasoning low\|medium\|high\|max\|up\|down` | Reasoning slider (mirrors variants; highest = default) |
| `/contextlimit up\|down\|<n>k` | Per-chat context limit (66k→1M) |
| `/thinklimit on\|off\|up\|down\|<n>k` | Thinking CONTEXT WINDOW (shown as "Think context"): how much of the recent chat the model may use for thinking (0→666k) |
| `/dynamiccontext on\|off` | Dynamic context mode (OFF default): recent context within the context-limit budget; hard-turns the thinking context OFF |
| `/setup save\|load\|list\|delete <name>` | Full button-setup snapshots (save/load whole button states) |
| `/ramlimit on\|off\|up\|down\|<n>GiB` | /tmp/opencodebox RAM cap (1→12GiB, off default) |
| `/writespeed up\|down\|<n>%` | Writing speed (25→200%, default 100%): <100 slower pacing, >100 turbo/terse |
| `/thinking` | Show/hide thinking blocks in the TUI (keybind `<leader>t`) |
| `/askquestions on\|off` | Asking questions. OFF = never ask, decide yourself |
| `/memorycollect on\|off` | Memory collection |
| `/memoryuse on\|off` | Memory usage (priority source #1) |
| `/agentsmodes on\|off` | Layered AGENTS.md vs classic AGENTS.md |
| `F2` | Cycle recent models; `<leader>f` cycle favourite models |

The **button bar** window (opens with the terminal) shows all of these with
their live state, sliders for the limits, context/spend readout and MCP
status — its keys: `1-6 p d q m u a c t g ← → o v f T b x ?`.

Workflow commands: `/grill <topic>`, `/wayfinder <idea>`,
`/ponytail [lite|full|ultra]`, `/memory`, `/backup`,
`/git <Project> "msg"`, `/gitall`, `/limits`, `/status`, `/projects`,
`/session-history`, `/force`, `/compact` (native auto-compaction).

Every toggle persists: the next session starts with the same button state.
