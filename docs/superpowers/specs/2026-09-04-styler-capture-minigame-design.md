# Ranger-Style Styler Capture Minigame Design

**Goal:** A beat/rhythm-based capture minigame, in the spirit of the Pokémon Ranger games, usable as a special-case alternative to a normal battle+Poké Ball capture for scripted encounters (boss-style legendaries, sidequests, "Ranger Station" events, etc.).

**Explicitly out of scope for this pass:**
- Replacing or altering normal wild encounters/battling in any way. Nothing about the default catch flow changes.
- Species/flag-driven auto-triggering from the wild encounter tables. Triggering is scripted-event-only (see Trigger below); table-driven triggering is a possible future follow-up, not part of this design.
- Magikarp Jump-style forms/minigame (separate idea, shelved for now, no dependency here).
- New sprite/creature art. The target Pokémon is represented by its existing icon/portrait; no new art is authored as part of this work.
- Multiplayer/link support (unlike `pokemon_jump.c`, which is link-aware).
- A global config flag gating this feature on/off. It's opt-in per script call (an author only gets it by using the new script commands), not a behavior switch on existing systems, so no config define is needed.

---

## Trigger & scripting API

Mirrors the existing `setwildbattle` / `dowildbattle` pair (`src/scrcmd.c`, `data/script_cmd_table.inc`):

- **`setstylercapture SPECIES LEVEL`** — stages a scripted mon the same way `CreateScriptedWildMon` does for `setwildbattle` (species/level, default IVs/nature/moves). No double-encounter variant.
- **`dostylercapture`** — starts the minigame: `StartStylerCapture(CB2_ReturnToField)`, following `BattleSetup_StartScriptedWildBattle`'s pattern of swapping in a dedicated `CB2`/task-driven mode and calling `ScriptContext_Stop()` until it returns.
- On return to the map, `VAR_RESULT` is set to `STYLER_RESULT_CAUGHT` or `STYLER_RESULT_FAILED` so the calling script can branch (`if.compare VAR_RESULT ...`), exactly like scripts already branch on `gBattleOutcome`-derived results after `dowildbattle`.

Both new commands are added to `data/script_cmd_table.inc` alongside the existing wild-battle entries and get constants generated into `include/constants/script_commands.h` via the normal `make_scr_cmd_constants.py` pipeline — no manual constant-numbering.

## Core gameplay loop

A dedicated full-screen minigame (own BG setup + task-based state machine), following the `src/pokemon_jump.c` / `src/mining_minigame.c` convention rather than hooking into the battle engine (a battle-engine hook was considered and rejected — the battle state machine, AI, and turn order aren't built to host an unrelated minigame mid-turn, and bending it to do so would be far more invasive than a standalone screen).

**On-screen elements** (existing UI primitives only — no new creature art):
- The target's existing icon/portrait.
- A beat bar (visual metronome).
- A capture gauge (fills toward capture).
- A styler-energy meter (drains toward failure).

**Loop:**
1. Intro/countdown, reusing `minigame_countdown.h` like the other minigames do.
2. A repeating beat plays at a tempo set by difficulty (see below).
3. **Normal beat:** press A on-beat → gauge fills by a fixed amount. Miss → energy drains by a fixed amount.
4. **Attack beat** (a periodic, distinctly-cued beat): a directional input is required instead of A, standing in for dodging the Pokémon's attack. Missing it drains energy by *more* than a normal miss.
5. **Capture:** gauge reaches full → immediate success. No secondary catch-rate RNG roll is made — filling the gauge *is* the catch, matching how capture works in the actual Ranger games (loop count fills the target's capture rings, no separate ball-throw-style probability check).
6. **Failure:** energy reaches zero → the styler "breaks," the encounter ends in failure. No fainting, no consumed items, no other side effect — the Pokémon simply isn't caught.

## Difficulty scaling

Derived once at start-up from the staged mon's `catchRate` (`include/pokemon.h`'s `struct BaseStats.catchRate` field, same source `setwildbattle`'s catch odds ultimately trace back to) and level:

- Lower `catchRate` and/or higher level → larger gauge-fill requirement, faster beat tempo, higher energy-drain-per-miss, and more frequent attack beats.
- These four axes are derived from a single difficulty formula (not four independently-invented constants) so a hack author only reasons about one effective "difficulty" dial per Pokémon; exact curve/tuning constants are worked out during implementation and testing, not fixed in this spec.

## Capture resolution

On success, the newly-caught mon is handed to the player through the **same post-catch pipeline a thrown Poké Ball already uses** — nickname prompt, Pokédex registration, sent to PC if the party is full — rather than a second, parallel implementation of "give player a Pokémon." This is a reuse point to identify precisely during implementation (the code path invoked after a successful catch in the battle engine) so the minigame calls into it directly instead of duplicating its behavior.

## Architecture / file plan

- `src/styler_capture.c` + `include/styler_capture.h` — new files, isolated from other systems per the project's "minimally invasive" style guidance.
- Internal structure follows `pokemon_jump.c`'s conventions: an enum of `FUNC_*` task states (intro → countdown → beat round → win/lose → exit), its own BG layer assignments, its own window(s) for the gauge/meter UI.
- `src/scrcmd.c` — two new `ScrCmd_setstylercapture` / `ScrCmd_dostylercapture` functions, next to `ScrCmd_setwildbattle` / `ScrCmd_dowildbattle`.
- `data/script_cmd_table.inc` — two new entries.
- A small staging struct (mirroring whatever `CreateScriptedWildMon` populates) holds species/level between the `set*` and `do*` calls.

## Testing

- New tests under `test/` exercising the difficulty formula (catch-rate/level → gauge size, tempo, drain rate, attack-beat frequency) as pure functions, decoupled from the task/BG machinery so they don't need the full test-ROM rendering path.
- Manual verification in mGBA for the actual feel of the beat timing, following this project's existing pattern of hand-tuning minigame feel (there's no automated way to assert "does this feel like Ranger").
- A minimal map script wiring `setstylercapture` → `dostylercapture` → branch on `VAR_RESULT`, used both as a manual smoke test and as the reference example for how hack authors are expected to call this.
