# Ranger Capture Styler Extensions Design

> **Revision note:** This spec originally designed a from-scratch minigame before discovering `src/ranger_capture.c` already exists (823 lines, committed `c536bcdfa2`) — a complete, working DDR-style rhythm minigame wired into the ball-throw battle flow via `ITEM_CAPTURE_STYLER` (`B_RANGER_CAPTURE` config, on by default). This revision keeps that implementation as the base and designs three additions on top of it instead of a parallel system.

**Goal:** Extend the existing Ranger Capture minigame with three additions: attack/obstacle notes (the target fights back), level-based difficulty scaling, and a standalone scripted-encounter trigger that runs the minigame as an alternative to a battle entirely (not a modification of the battle engine).

**Explicitly out of scope for this pass:**
- Any change to the battle turn/action-selection engine (`HandleTurnActionSelectionState` and friends). The scripted trigger is a standalone screen, not a way to auto-play a battle turn — that path was considered and rejected as disproportionate surgery on the project's most complex, most bug-prone subsystem for what should be an alternative to a battle.
- Changing how `ITEM_CAPTURE_STYLER` behaves when thrown in a normal battle. That entry point is untouched except for gaining the two gameplay extensions (attack notes, level scaling) via the shared core.
- New sprite/tile art. Everything continues to use the existing solid-color tile approach already in `sRangerBgTiles`.
- Magikarp Jump wild-encounter work (separate spec, tracked independently).

---

## Architecture: split the existing file into a shared core + two entry points

Today, `RangerCapture_Init` (`src/ranger_capture.c`) and its state machine read battle globals directly (`gBattlerTarget`, `gBattleMons[gBattlerTarget]`) both to compute difficulty and, in `battle_script_commands.c`, to hand over the caught mon. To support a standalone (non-battle) entry point without duplicating ~800 lines, the difficulty inputs are extracted into a plain struct the core no longer sources from globals itself:

```c
struct RangerCaptureParams {
    u32 catchRate;
    u8  level;
    bool8 isAsleep;
    bool8 isFrozen;
    bool8 isLowHp;    // < 25% max HP
};
```

- **`CalculateDifficulty`** changes signature to `CalculateDifficulty(struct RangerCaptureParams *params)` and reads fields from it instead of `gBattleMons[gBattlerTarget]` directly. Its existing tiering logic (catch-rate → loopsNeeded/speed/maxMisses, easier when asleep/frozen/low-HP) is unchanged, just re-pointed at the struct.
- **Battle entry point** (existing behavior, `battle_script_commands.c`'s ball-throw handling is unchanged): a thin wrapper builds the params from `gBattleMons[gBattlerTarget]` before calling into the shared init, exactly reproducing today's values.
- **Standalone entry point** (new): builds params from a staged species/level with `isAsleep`/`isFrozen`/`isLowHp` all `FALSE` (a scripted encounter's target isn't already mid-battle-afflicted) — a deliberate simplification, not a gap, since there's no live battle mon to read status from.

A `u8 resultMode` field (`RANGER_RESULT_MODE_BATTLE` / `RANGER_RESULT_MODE_SCRIPTED`) stored on `struct RangerCapture` tells `DoExit` which of the two finish paths below to take. Everything else in the state machine (`DoSetupGfx`, `DoCountdown`, `DoPlaying`, `DoSuccessAnim`, `DoFailAnim`, `DoFadeOut`) is shared, unmodified control flow.

## Extension 1: Attack/obstacle notes

A new note kind representing the target fighting back mid-loop. `struct RangerNote` gains a `kind` field (`NOTE_KIND_NORMAL` / `NOTE_KIND_ATTACK`), rendered with a distinct tile/color so it reads as different at a glance.

- **Spawning:** `SpawnNote` gains a difficulty-scaled chance to spawn an attack note instead of normal (a new `attackNoteChance` percentage set in `CalculateDifficulty` — higher for tougher/lower-catch-rate targets, consistent with the existing "harder target = more of X" pattern already used for loop count/speed/miss allowance).
- **Correct play is the opposite of a normal note:** pressing the matching D-pad direction while an attack note occupies the hit zone means getting hit — a harsher penalty than a normal miss (bigger `loopProgress` loss and/or an extra miss count), reusing `HandleInput`'s existing nearest-note lookup but branching on `kind` once a note is found.
- **Letting it pass is correct:** in `UpdateNotes`, a normal note reaching `LANE_END_COL` unhandled is today's miss; an attack note reaching `LANE_END_COL` unhandled is a successful dodge — no penalty, small `loopProgress` credit for landing it cleanly.

## Extension 2: Level scaling

`CalculateDifficulty` currently scales off catch rate plus in-battle easing (asleep/frozen, low HP) but never looks at level. Add a level-based adjustment to `noteSpeed` (the single "how fast/hard" dial the rest of the function already funnels into), applied after the catch-rate tier is picked and before the existing eases: higher-level targets nudge `noteSpeed` down (faster notes, matching the direction lower catch rate already pushes it), clamped so it never drops below the tier's existing floor. This keeps level as a modifier on the same difficulty axis rather than a second independent system.

## Extension 3: Standalone scripted trigger

Mirrors `setwildbattle`/`dowildbattle` (`src/scrcmd.c`, `data/script_cmd_table.inc`) exactly, per the original design:

- **`setstylercapture SPECIES LEVEL`** — stages species/level in two static file-scope variables (no live `struct Pokemon`/party slot needed until a successful capture).
- **`dostylercapture`** — calls the new standalone entry point, which runs the *exact same* rhythm-game state machine as the battle path (with the extensions above included), just started with params built from the staged species/level and `isAsleep`/`isFrozen`/`isLowHp` all false.
- **On success**, the standalone exit path builds the mon (`CreateMonWithIVs` + `GiveMonInitialMoveset`, mirroring `CreateScriptedWildMon`) and calls `GiveScriptedMonToPlayer(&mon, PARTY_SIZE)` — the same non-battle "give player a Pokémon" pipeline `ScriptGiveMon`/egg hatching/the Game Corner gacha already use, handling party-vs-PC placement and Pokédex seen/caught flags. **On failure**, nothing happens beyond ending the encounter (no fainting, no consumed resources).
- **Returning to the script:** `SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic)` (the same callback `dowildbattle` returns through), after stashing the outcome in a static. A new `special`, `GetStylerCaptureOutcome`, returns it — registered in `data/specials.inc` — so a calling script reads the result the same way existing scripts already do after `dowildbattle`:
  ```
  dostylercapture
  specialvar VAR_RESULT, GetStylerCaptureOutcome
  goto_if_eq VAR_RESULT, STYLER_RESULT_CAUGHT, MyScript_Success
  ```

This is a genuine alternative to a battle — no battle state (`gBattleMons`, `gBattlerTarget`, `gBattleTypeFlags`) is touched or created, and nothing about `HandleTurnActionSelectionState` or any other battle-engine code changes.

## Testing

- Unit tests (`test/` `TEST()` macro, same pattern as `test/fpmath.c`) for `CalculateDifficulty` as a pure function of `struct RangerCaptureParams` — covering the catch-rate tiers, the asleep/frozen/low-HP eases, the new level adjustment, and `attackNoteChance`'s scaling — decoupled from the BG/task machinery.
- Manual mGBA verification for feel (note timing, attack-note tension, the standalone screen's countdown/win/lose flow) — there's no automated way to assert "does this feel right," consistent with how the existing minigame was itself hand-tuned.
- A minimal reference map script (`setstylercapture` → `dostylercapture` → branch on `VAR_RESULT`) as both a manual smoke test and the example hack authors copy.
