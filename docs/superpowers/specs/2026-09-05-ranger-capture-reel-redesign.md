# Ranger Capture: Reel + Closing Ring Redesign

**Goal:** Replace the Ranger Capture Styler's current 4-lane DDR display with a single rhythm reel (mixed D-pad/A/B icons, plus attack icons to avoid) and a real GBA hardware-scaled ring that closes around the target's actual overworld sprite — closer to how the Pokémon Ranger DS games read, adapted honestly to what a D-pad-and-buttons device can actually do.

**Why not a literal Ranger port:** Ranger's mechanic is a continuous stylus path — the DS reads an (x,y) coordinate stream and does real-time geometry to detect a closed loop drawn around a moving target, with a second screen dedicated to the effort. GBA has no pointer input at all (only discrete D-pad/button states) and one screen. A rotation-sequence "trace a circle with the D-pad" mechanic was considered and rejected as a confusing false-equivalence — it doesn't reconstruct anything geometric, it's just an arbitrary discrete pattern wearing the same name. A rhythm reel is the honest adaptation: it's a real skill test the hardware can actually express, and the ring gives the visual read of "closing in" without pretending to be the input method that isn't there.

**What "closing ring" means here, corrected against the real games:** In Ranger, a closed loop's progress is never undone — a wild Pokémon's attack, if it lands while you're mid-loop, cancels the *in-progress* loop and drains a separate Styler-energy meter, but loops you've already banked stay banked. So here: a miss stalls the ring (no further shrink that beat) and resets the current loop's progress-within-that-loop to its start, but never un-bans a `LOOPS` count already reached, and never reduces the ring past where it already legitimately closed to. What actually ends the attempt is a separate miss/energy gate — the ring closing is a readout of progress, not the fail condition itself.

**Explicitly out of scope for this pass (first slice, per direct instruction):**
- L/R joining the reel's icon pool at higher difficulty — a real, agreed idea, but deferred; this pass ships with D-pad Up/Down/Left/Right + A + B + the attack icon.
- Any battle-engine changes. This still only touches `src/ranger_capture.c` and its existing entry points (battle-item throw, standalone script trigger) — both already built and working.
- Changing `setstylercapture`/`dostylercapture`, `GetStylerCaptureOutcome`, or the give-mon-to-player standalone success path — none of that is touched.

---

## What's kept as-is

The existing `RSTATE_*` task state machine (`RSTATE_INIT` → `RSTATE_SETUP_GFX` → `RSTATE_COUNTDOWN` → `RSTATE_PLAYING` → `RSTATE_SUCCESS_ANIM`/`RSTATE_FAIL_ANIM` → `RSTATE_FADE_OUT` → `RSTATE_EXIT`), the dual battle/standalone entry points (`RangerCapture_Init`/`RangerCapture_InitStandalone` → `RangerCapture_InitCommon`), `CalculateDifficultyForMode`/`ComputeRangerCaptureDifficulty`'s tiering (catch rate → `loopsNeeded`/`noteSpeed`/`maxMisses`/`attackNoteChance`, level scaling, incapacitated/low-HP eases), and the `loopProgress` (0–100 per loop) + `HIT_PERFECT`/`HIT_GOOD`/`HIT_OK`/`HIT_MISS` scoring tiers all carry over unchanged in spirit. What changes is how progress is delivered (one reel instead of four lanes) and how it's drawn (a real sprite + hardware-scaled ring instead of BG-tile bars).

## The reel

One horizontal track (replaces the four `LANE_UP_ROW`/`LANE_RT_ROW`/`LANE_DN_ROW`/`LANE_LT_ROW` rows). Icons still scroll left-to-right toward a single fixed hit zone, same motion/timing model as today (`noteSpeed` frames per column, `LANE_START_COL`→`LANE_END_COL`), just one track instead of four.

**Icon pool for this pass:** Up, Down, Left, Right, A, B, and Attack — each its own solid color tile (same "no new art, just distinct colors" approach the current lanes already use): Up/Right/Down/Left keep their existing colors (yellow/green/blue/red) for continuity with anything already documented about this feature; A and B get two new colors; Attack keeps the existing magenta (`COL_ATTACK`). `SpawnNote`'s random pick now draws from this whole pool (weighted so Attack still only appears at `attackNoteChance`, same as today) instead of being implicitly "whichever lane."

**Input:** `HandleInput` reads whichever of `DPAD_UP/DOWN/LEFT/RIGHT/A_BUTTON/B_BUTTON` was newly pressed, finds the nearest icon *of that same type* to the hit zone (same nearest-neighbor distance logic as today, just matching on icon identity instead of lane), and judges Perfect/Good/OK/Miss by the same distance thresholds already in place. Pressing while the nearest icon in range is an Attack icon is still the punish case (bigger `loopProgress`... see below) exactly as today.

## The ring (replaces the tile-based loop meter)

Modeled directly on the existing `gThinRingShrinkingAffineAnimCmds`/`gThinRingShrinkingSpriteTemplate` pattern already in `src/battle_anim_effects_2.c` — a `struct SpriteTemplate` with an affine-enabled `OamData`, driven by a `union AffineAnimCmd[]` script (`AFFINEANIMCMD_FRAME` for the initial absolute scale, a relative frame spread over N ticks for a smooth shrink, `AFFINEANIMCMD_END_ALT`). GBA hardware does the actual per-pixel scaling — no manual redraw loop.

- **Radius reflects `loopProgress`** (0–100 within the current loop): on every scoring hit (Perfect/Good/OK), re-trigger the ring's affine anim (`StartSpriteAffineAnim`) targeting the new, smaller scale for the updated `loopProgress`. On loop completion (`loopProgress` reaches 100 → `LOOPS`+1, `loopProgress` resets to 0), the ring's anim retargets back out to its starting (largest) scale for the next loop.
- **A miss does not shrink further and does not grow the ring back out.** It resets `loopProgress` to 0 for the *current, not-yet-completed* loop (ring's next scored hit targets the ring back toward its current-loop starting scale, same as if the loop had just begun) — `LOOPS` already banked is untouched, matching the corrected Ranger behavior above. This replaces today's `loopProgress -= 15`/`-25` on miss; the separate `missCount`/`maxMisses` counter (unchanged) is still what actually ends the attempt when exceeded.
- The ring is centered on the target's sprite (below), using its own screen-space position, sized independent of the reel.

## The target's sprite

The target's real **overworld** sprite (not its front battle sprite, not its menu icon), following `src/pokemon_sprite_visualizer.c`'s `DrawFollowerSprite` precedent exactly: `graphicsId = species + OBJ_EVENT_MON` (plus shiny/female offsets if applicable) passed to `CreateObjectGraphicsSprite(graphicsId, callback, x, y, subpriority)`, then the same `oam.shape`/`oam.size`/`images`/`anims`/`subspriteTables` fixup from `SpeciesToGraphicsInfo()` the visualizer does defensively. This depends on `OW_POKEMON_OBJECT_EVENTS` (confirmed `TRUE` by default in this fork's `include/config/overworld.h`). A simple idle/facing animation (reuse `GetMoveDirectionAnimNum`-style facing, no new movement logic) is enough — this isn't a walking NPC, just a still target with its real, correct sprite.

No new sprite/OAM/VBlank infrastructure is needed: `ranger_capture.c`'s existing `RangerCapture_VBlankCB` (`LoadOam`/`ProcessSpriteCopyRequests`/`TransferPlttBuffer`) and `RangerCapture_InitCommon` (`ResetSpriteData`/`FreeAllSpritePalettes`) already set up everything a normal sprite-using screen needs — the file just never calls `CreateSprite` today. Confirmed via direct inspection: zero `CreateSprite`/`gSprites[]` references currently exist in `ranger_capture.c`.

## Testing

- The existing 8 `ComputeRangerCaptureDifficulty` unit tests are unaffected (this redesign doesn't touch that function) — they keep passing as a regression check.
- No new unit tests are feasible for the reel/ring rendering itself (sprite/OAM/affine state isn't something the `TEST()` framework can assert on meaningfully) — manual mGBA/headless-capture verification is the acceptance path, same as the note-based version before it.
- Manual verification checklist for the implementer: reel icons scroll and are visually distinguishable by color; pressing the matching button at the hit zone scores Perfect/Good/OK and shrinks the ring; pressing (rather than avoiding) an Attack icon punishes; a miss stalls the ring without growing it back out; completing enough loops triggers the existing success path; running out of misses triggers the existing failure path; the target's overworld sprite renders correctly (right species, right season/shiny state if applicable).
