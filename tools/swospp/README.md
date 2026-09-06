# SWOS++ — swos-port helper edition

> **Internal compatibility tool — not the normal SWOS++ release.**

This tree is a special-purpose SWOS++ fork kept inside `swos-port`. It exists to run the original DOS game as a behavioral oracle for `swos-port`, primarily to:

- record `.rgd` data used by `RecordedDataTest`;
- replay recordings and compare DOS and port behavior;
- inspect original game state and diagnose compatibility discrepancies;
- capture other reference data needed during port development.

It is not intended for ordinary play or distribution. Port-specific changes may deliberately alter or disable normal SWOS++ behavior to make recordings deterministic and comparable.

## Relationship to normal SWOS++

The normal version intended for DOS SWOS is maintained separately at <https://github.com/zlatkok/swospp>.

This helper edition belongs in the `swos-port` repository and should be committed and pushed there normally. Do **not** push this helper tree, or its port-only commits, to the separate `github.com/zlatkok/swospp` repository. If a generally useful fix should be shared with normal SWOS++, isolate and review that fix independently; do not merge or copy the helper tree wholesale.
