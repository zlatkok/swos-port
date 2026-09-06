# Automatic menu testing

The automatic menu tester is being introduced incrementally because invoking an arbitrary menu callback is not
inherently safe. Some callbacks start matches, write files, alter global video/audio configuration, or require a
specific competition state. Static discovery and durable progress tracking are therefore kept separate from the
dynamic walker.

## Inventory

Run:

```powershell
python tools\menuTestInventory.py
```

The script scans packed menus in `swos/swos.asm` and converted `.mnu` menus, then updates
`tests/data/menu-walker-inventory.json`. Existing status, fixture recipes, manual cases, policies and notes are
preserved when the inventory is regenerated.

Each menu starts in `discovered` state. The intended progression is:

1. `discovered` -- found statically but not yet activated;
2. `opens` -- a deterministic fixture can display it without an assertion or exception;
3. `walked` -- every safe, visible entry and option has been exercised;
4. `visual-reviewed` -- its generated snapshots passed automated checks and human review.

`reachable_from_main` records whether normal traversal can reach the menu. A menu that needs a particular team,
competition phase, saved file or match state instead gets a named `fixture`. `manual_cases` hold stable additional
entry sequences for paths that automatic traversal misses.

Global audio and video menus default to blocked policies. Their in-game-only options can later be allow-listed
individually, but the walker must not change display mode, fullscreen resolution, audio device or global volume.

## Dynamic walker phases

The C++ test harness already supports deterministic menu callbacks, entry inspection and selection, mocked input,
exceptions/assertions, and BMP snapshots. The walker will build on those facilities in these phases:

1. enumerate the active menu and record entry type, visibility, enabled state and callback identity;
2. open known-safe submenus from the main menu and return after each edge;
3. cycle boolean, number and string-table options, restoring their original values afterward;
4. add fixture constructors for career, competition, team-selection and in-match menus;
5. add explicit manual paths selected by inventory case ID;
6. process snapshots for text outside entry rectangles, unexpected clipping/elision, overlaps and blank regions.

Coverage reports are retained between runs and compared by menu/callback identity. Line coverage remains useful for
finding callbacks that were entered but only partially exercised; the inventory is the authoritative record of menu
and entry coverage.

## DOS/SWOS++ comparison

SWOS++ can provide reference captures for unchanged DOS menus. Those captures should be indexed by the same stable
menu/case IDs, but comparisons cannot be strict pixel equality after replacement fonts, procedural pitch sprites and
high-resolution rendering. Geometry, text content, selected entry, visibility and transition structure are better
cross-version assertions; image comparison should use masks or structural features.
