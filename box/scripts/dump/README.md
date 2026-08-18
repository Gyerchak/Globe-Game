# dump/ — one-time helper files made by OpenCodeBox

Files in here are ONE-TIME scripts/outputs the box itself generated while
working (migrations, one-off fixes, experiments). They act like helpers but
are not meant to be reused. Stable, regular tooling belongs in `tools/`;
occasionally-useful scripts belong in `helpers/`.

Anything no longer needed goes to `box/waste/` (the only allowed delete
method) — never `rm`.
