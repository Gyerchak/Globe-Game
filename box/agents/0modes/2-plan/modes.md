# Mode: plan

Read-only planning mode — the classic plan agent.

## Permissions (mirrors the classic plan mode)

```
edit:      deny    # no file changes
write:     deny
shell:     ask     # only with approval, read-only commands preferred
read:      allow
webfetch:  allow
websearch: allow
```

Explore, analyse, produce a plan. Never edit. Point out risks and options.
