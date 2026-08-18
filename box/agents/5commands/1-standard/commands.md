# Commands & buttons

This box exposes **buttons** as slash commands (and some as keybinds). At the
start of a new chat, briefly show the user these key controls:

| Command | What it changes |
|---|---|
| `/modes` | Agent-mode instruction set (layer 0modes) |
| `/logics` | Logic-style instruction set (layer 1logics) |
| `/knowledges` | Knowledge set (layer 2knowledges) |
| `/instructions` | Workflow set (layer 3instructions) |
| `/characters` | Persona (layer 4characters) |
| `/commands` | This command catalogue (layer 5commands) |
| `/deepthinking on\|off` | Deep thinking. OFF = token/money-saving mode |
| `/reasoning low\|medium\|high\|ultra` | Reasoning effort level |
| `/contextlimit up\|down\|<n>k` | Per-chat context limit (66k→1M) |
| `/thinklimit on\|off\|up\|down\|<n>k` | Thinking window limit (0→666k) |
| `/thinking` | Show/hide thinking blocks in the TUI (keybind `<leader>t`) |
| `/memorycollect on\|off` | Memory collection |
| `/memoryuse on\|off` | Memory usage (priority source #1) |
| `/agentsmodes on\|off` | Layered AGENTS.md vs classic AGENTS.md |
| `F2` | Cycle recent models; `<leader>f` cycle favourite models |

Workflow commands: `/grill <topic>`, `/wayfinder <idea>`,
`/ponytail [lite|full|ultra]`, `/handoff [focus]`, `/memory`, `/backup`,
`/git <Project> "msg"`, `/gitall`, `/limits`, `/status`, `/projects`,
`/session-history`, `/force`, `/continue`.

Every toggle persists: the next session starts with the same button state.
