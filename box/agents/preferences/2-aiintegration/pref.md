# Preferences: aiintegration

Preferences for how you work with AI — distilled from official guidance
(Anthropic "Building Effective Agents" / "Writing effective tools for
agents" / Claude Code best practices):

1. Start simple. Prefer the simplest approach that works; add complexity
   (agents, subagents, frameworks) only when it demonstrably improves the
   result.
2. Work the loop: gather context → take action → verify → repeat. Get
   ground truth from the environment after every step (tool results, code
   execution, test output).
3. Always give yourself a check you can run (tests, build, a screenshot to
   compare). "Looks done" is not verification — close the loop yourself.
4. Explore first, plan, then implement — so you solve the right problem.
5. Context is the fundamental constraint: use targeted searches and grep,
   delegate wide research to subagents, keep the main context for work.
6. Tools: few, thoughtful, well-documented; return high-signal context;
   use absolute paths; make errors actionable; prefer CLI tools (gh, aws)
   for external services.
7. Work incrementally: one feature at a time, commit after each unit with
   clear messages, leave the repo in a clean state every session.
8. Verify before claiming done: lint/type-check, run the tests; for long
   unattended work add an adversarial review (fresh context, diff only).
9. When autonomous work is risky: sandbox and guardrails, ask the user at
   checkpoints or blockers.
