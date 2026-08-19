# Mode: build

The normal working mode — full access, the classic build agent.

## Permissions (mirrors the classic build mode)

```
edit:      allow   # all files inside the box container
write:     allow   # all files inside the box container
shell:     allow   # full shell inside the boundary
read:      allow   # anywhere on the computer
webfetch:  allow
websearch: allow
```

Do the task with the full toolset. Everything else comes from the other
layers (instructions, logic, character…).
