# Ranger Capture: Closing Ring + Target Sprite (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a GBA hardware-scaled ring that closes around the target's real overworld sprite, driven by the Ranger Capture minigame's existing `loopProgress`/`loopsCompleted` state — with zero changes to the current note-lane gameplay logic underneath it.

**Architecture:** Two new OAM sprites in `src/ranger_capture.c`: the target's overworld sprite (via `CreateObjectGraphicsSprite`, following `src/pokemon_sprite_visualizer.c`'s existing non-field precedent) and a ring sprite reusing this project's own existing `graphics/battle_anims/sprites/thin_ring.png` asset, scaled every frame via a per-frame sprite callback that reads `sRanger->loopProgress` directly and writes the sprite's OAM affine matrix (the same low-level `ObjAffineSet`/`SetOamMatrix` pattern already used in `src/game_corner_flappybird.c`). No new art, no changes to `SpawnNote`/`UpdateNotes`/`HandleInput`/`BuildInitialTilemap`.

**Tech Stack:** C (arm-none-eabi via devkitARM), GBA OAM/affine sprite hardware, `mgba-headless` + Lua scripting for visual verification (no faster feedback loop exists for this kind of change).

---

## Spec coverage map

(From `docs/superpowers/specs/2026-09-05-ranger-capture-reel-redesign.md` — this plan covers only the ring + sprite portions of that spec; the reel/icon rewrite is explicitly deferred to a Phase 2 plan.)

- Ring closes around the target, GBA hardware-scaled, modeled on the existing `thin_ring` asset → Tasks 1-3
- Target's real overworld sprite (not front sprite, not icon) → Task 4
- Ring reflects `loopProgress`, resets outward on loop completion, stalls (not grows back) on a miss → Task 5
- Manual verification via headless capture → Task 4

---

### Task 1: Ring sprite scaffolding (OAM data, graphics loading, sprite creation)

**Files:**
- Modify: `src/ranger_capture.c`

This task adds the ring sprite to the screen at a fixed (not-yet-animated) size, proving the asset loads and displays correctly before any scaling logic is added.

- [ ] **Step 1: Add the includes and extern declarations for the existing ring asset**

Add near the top of `src/ranger_capture.c`, alongside the existing includes:

```c
#include "sprite.h"
#include "decompress.h"
```

(`sprite.h` for `struct SpriteTemplate`/`struct OamData`/`CreateSprite`/`ObjAffineSet`/`SetOamMatrix`/`LoadSpritePalette` — the file doesn't currently declare any sprites, so this is new. `decompress.h` for `LoadCompressedSpriteSheetUsingHeap`/`struct CompressedSpriteSheet`, used in Step 4 below.)

Add these `extern` declarations near the top of the file (real, existing global arrays defined in `src/graphics.c:1064-1065`, reused as-is — no new art):

```c
extern const u32 gBattleAnimSpriteGfx_ThinRing[];
extern const u16 gBattleAnimSpritePal_ThinRing[];
```

**Step 2: Define a local OAM template and sprite palette/sheet tags for the ring**

Add near the top of the file, with the other layout constants:

```c
// Sprite palette/tile tags for the capture ring (local to this screen - not shared with battle anims)
#define RING_TILE_TAG 0xF001
#define RING_PAL_TAG  0xF001

// Ring shrinks from RING_SCALE_MIN (largest on-screen, loop just started) to
// RING_SCALE_MAX (smallest/tightest, loop complete). GBA affine scale is an
// inverse divisor - a SMALLER value here makes the sprite appear LARGER on screen.
#define RING_SCALE_MIN 0x60
#define RING_SCALE_MAX 0x180
```

Add a local OAM template (mirrors `gOamData_AffineDouble_ObjBlend_64x64`'s shape/size/affine settings from `src/data/battle_anim.h:883-890`, defined locally instead of reusing the battle-anim extern to keep this screen self-contained per this project's isolate-new-code style):

```c
static const struct OamData sRingOamData =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_DOUBLE,
    .objMode = ST_OAM_OBJ_BLEND,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};
```

A minimal affine-anim table is required so `CreateSprite` successfully allocates an OAM affine matrix for this sprite (via `InitSpriteAffineAnim`, which every affine-mode sprite goes through) — its actual content is irrelevant since Task 3 overwrites the matrix every frame directly, but the pointer must be valid:

```c
static const union AffineAnimCmd sRingAffineAnimCmds[] =
{
    AFFINEANIMCMD_FRAME(0x100, 0x100, 0, 0),
    AFFINEANIMCMD_END,
};

static const union AffineAnimCmd *const sRingAffineAnimTable[] =
{
    sRingAffineAnimCmds,
};
```

**Step 3: Add a forward-declared sprite callback and the sprite template**

```c
static void SpriteCB_CaptureRing(struct Sprite *sprite);

static const struct SpriteTemplate sRingSpriteTemplate =
{
    .tileTag = RING_TILE_TAG,
    .paletteTag = RING_PAL_TAG,
    .oam = &sRingOamData,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = sRingAffineAnimTable,
    .callback = SpriteCB_CaptureRing,
};
```

(`gDummySpriteAnimTable`, declared `extern const union AnimCmd *const gDummySpriteAnimTable[];` in `include/sprite.h:260`, is this project's standard empty-anim-table constant for non-animated sprites — e.g. `src/game_corner_blackjack.c:1015` uses it the same way.)

**Step 4: Add the ring sprite ID to `struct RangerCapture` and create/destroy it**

Add to the struct:

```c
struct RangerCapture {
    u8  rstate;
    u8  resultMode;
    ...
    u8  ringSpriteId;
    struct RangerNote notes[MAX_NOTES];
};
```

In `DoSetupGfx` (`src/ranger_capture.c`), after the existing `ShowBg(0);` call and before `SetGpuReg(REG_OFFSET_DISPCNT, ...)`, add:

```c
    {
        const struct CompressedSpriteSheet ringSheet = {
            .data = gBattleAnimSpriteGfx_ThinRing,
            .size = 0x0800,
            .tag = RING_TILE_TAG,
        };
        const struct SpritePalette ringPalette = {
            .data = gBattleAnimSpritePal_ThinRing,
            .tag = RING_PAL_TAG,
        };
        LoadCompressedSpriteSheetUsingHeap(&ringSheet);
        LoadSpritePalette(&ringPalette);
    }
    sRanger->ringSpriteId = CreateSprite(&sRingSpriteTemplate, 152, 56, 0);
```

`gBattleAnimSpriteGfx_ThinRing` is `.smol`-compressed (`INCGFX_U32(..., ".4bpp.smol")` in `src/graphics.c:1064`) — `LoadCompressedSpriteSheetUsingHeap` is the confirmed loader for this (it's the same function `src/battle_anim.c:584` uses for every `.smol` battle-anim sprite sheet). `gBattleAnimSpritePal_ThinRing` is a plain `.gbapal` array (not compressed), so the ordinary `LoadSpritePalette` (`include/sprite.h:318`) is correct for it — do not reach for a "compressed palette" loader, there isn't one needed here.

(`152, 56` is a starting guess for screen-pixel position - centered horizontally in the play area to the right of the left info panel, above where the reel/lanes currently sit. This will need adjusting once Task 4's capture shows the actual result — note that as a placeholder now, not a final value.)

In `DoExit` (`src/ranger_capture.c`), before the existing `Free(sRanger);` call in BOTH the scripted and battle-mode branches, add:

```c
    DestroySprite(&gSprites[sRanger->ringSpriteId]);
    FreeSpriteTilesByTag(RING_TILE_TAG);
    FreeSpritePaletteByTag(RING_PAL_TAG);
```

**Step 5: Stub the sprite callback (fixed scale for now, animated scale comes in Task 3)**

```c
static void SpriteCB_CaptureRing(struct Sprite *sprite)
{
    // Placeholder: fixed neutral scale. Task 3 replaces this with a loopProgress-driven scale.
}
```

**Step 6: Build**

Run: `make -j$(nproc)`
Expected: builds successfully. Fix any include/loader mismatches found per the notes in Step 4 before moving on — do not proceed to Task 2 with a non-building tree.

**Step 7: Commit**

```bash
git add src/ranger_capture.c
git commit -m "Add the capture ring sprite scaffolding (fixed scale, no gameplay wiring yet)"
```

Do NOT add a "Co-Authored-By" trailer to the commit message.

---

### Task 2: Target's overworld sprite

**Files:**
- Modify: `src/ranger_capture.c`

- [ ] **Step 1: Add the include for overworld sprite creation**

```c
#include "event_object_movement.h"
```

**Step 2: Add fields to `struct RangerCapture` for the resolved target and its sprite**

```c
struct RangerCapture {
    ...
    u16 targetSpecies;
    u8  targetSpriteId;
    u8  ringSpriteId;
    struct RangerNote notes[MAX_NOTES];
};
```

**Step 3: Resolve and store the target species in `CalculateDifficultyForMode`**

`CalculateDifficultyForMode` (`src/ranger_capture.c`) already branches on `sRanger->resultMode` to read species for the catch-rate lookup. Extend both branches to also store it:

```c
static void CalculateDifficultyForMode(void)
{
    struct RangerCaptureParams params = {0};
    struct RangerDifficulty diff;

    if (sRanger->resultMode == RANGER_RESULT_MODE_SCRIPTED)
    {
        sRanger->targetSpecies = sStagedStylerSpecies;
        params.catchRate = gSpeciesInfo[sStagedStylerSpecies].catchRate;
        params.level = sStagedStylerLevel;
        params.isIncapacitated = FALSE;
        params.isLowHp = FALSE;
    }
    else
    {
        sRanger->targetSpecies = gBattleMons[gBattlerTarget].species;
        params.catchRate = gSpeciesInfo[gBattleMons[gBattlerTarget].species].catchRate;
        params.level = gBattleMons[gBattlerTarget].level;
        params.isIncapacitated = (gBattleMons[gBattlerTarget].status1 & STATUS1_INCAPACITATED) != 0;
        params.isLowHp = gBattleMons[gBattlerTarget].hp * 4 < gBattleMons[gBattlerTarget].maxHP;
    }

    diff = ComputeRangerCaptureDifficulty(params);
    sRanger->loopsNeeded = diff.loopsNeeded;
    sRanger->noteSpeed = diff.noteSpeed;
    sRanger->maxMisses = diff.maxMisses;
    sRanger->attackNoteChance = diff.attackNoteChance;
}
```

(Shiny/gender are deliberately not resolved here for this first slice — the overworld sprite always displays as the default non-shiny, non-female appearance. This is a scoped simplification, not an oversight; a follow-up can thread the actual personality/gender through once one exists at this point in the flow.)

**Step 4: Create the target sprite in `DoSetupGfx`**

Add this in `DoSetupGfx`, near where the ring sprite is created (Task 1 Step 4) — the overworld sprite creation is self-contained (loads its own graphics/palette internally), so order relative to the ring doesn't matter:

```c
    sRanger->targetSpriteId = CreateObjectGraphicsSprite(
        sRanger->targetSpecies + OBJ_EVENT_MON,
        SpriteCallbackDummy,
        152, 88, 1);
    gSprites[sRanger->targetSpriteId].oam.priority = 1;
```

(`SpriteCallbackDummy`, declared in `include/sprite.h:287` / implemented `src/sprite.c:768`, is this project's standard no-op sprite callback for static display sprites. `152, 88` places it below the ring's `152, 56` from Task 1 for now; both positions get tuned together once Task 4's capture shows the real layout. Subpriority `1` vs the ring's `0` and OAM priority `1` are placeholder layering choices — verify in Task 4 that the ring actually draws in front of/around the sprite rather than being hidden behind it, and adjust priority/subpriority if not.)

**Step 5: Destroy the target sprite in `DoExit`**

Alongside the ring's cleanup from Task 1 Step 4, in both `DoExit` branches:

```c
    DestroySprite(&gSprites[sRanger->targetSpriteId]);
```

(`CreateObjectGraphicsSprite` manages its own tile/palette tags internally via the object-event graphics system — unlike the ring's manually-tagged sheet/palette, do not call `FreeSpriteTilesByTag`/`FreeSpritePaletteByTag` for this one; `DestroySprite` alone matches how `pokemon_sprite_visualizer.c` tears down its own follower sprite preview.)

**Step 6: Build**

Run: `make -j$(nproc)`
Expected: builds successfully.

**Step 7: Commit**

```bash
git add src/ranger_capture.c
git commit -m "Show the target's real overworld sprite on the Ranger Capture screen"
```

Do NOT add a "Co-Authored-By" trailer to the commit message.

---

### Task 3: Drive the ring's scale from `loopProgress`

**Files:**
- Modify: `src/ranger_capture.c`

- [ ] **Step 1: Implement the real scaling callback**

Replace the Task 1 stub:

```c
static void SpriteCB_CaptureRing(struct Sprite *sprite)
{
    struct ObjAffineSrcData affineSrc;
    struct OamMatrix matrix;
    u16 scale = RING_SCALE_MIN + ((RING_SCALE_MAX - RING_SCALE_MIN) * sRanger->loopProgress) / LOOP_PROGRESS_MAX;

    affineSrc.xScale = scale;
    affineSrc.yScale = scale;
    affineSrc.rotation = 0;
    ObjAffineSet(&affineSrc, &matrix, 1, 2);
    SetOamMatrix(sprite->oam.matrixNum, matrix.a, matrix.b, matrix.c, matrix.d);
}
```

This runs automatically every frame the ring sprite exists (the normal sprite-processing loop calls every sprite's `callback` each frame) — nothing in `DoPlaying` needs to explicitly invoke it. Because it reads `sRanger->loopProgress` directly, it "reflects" progress and loop resets for free: whatever `HandleInput`/`UpdateNotes` already do to `loopProgress` today (increase it on hits, reset it to 0 on the loop-completion branch in `DoPlaying`) is picked up on the very next frame with no additional wiring.

**Step 2: Confirm the corrected miss behavior is already what this needs**

This plan does not change miss handling in `HandleInput`/`UpdateNotes` — those still subtract from `loopProgress` on a miss today (`-15`/`-25`, floored at 0), which is Phase 2's concern (the spec's "miss resets the current loop to 0, not a partial subtraction" correction is a note-system/scoring change, out of scope here). For Phase 1, the ring faithfully displays whatever `loopProgress` already is, however it got there — confirm this by reading the current `HandleInput`/`UpdateNotes` miss branches and note in your report that Phase 2 is where that scoring logic itself changes, not this task.

**Step 3: Build**

Run: `make -j$(nproc)`
Expected: builds successfully.

**Step 4: Re-run the existing difficulty tests**

Run: `make TESTS="RangerCapture" check`
Expected: PASS (8 tests — untouched by this task).

**Step 5: Commit**

```bash
git add src/ranger_capture.c
git commit -m "Drive the capture ring's scale from loopProgress"
```

Do NOT add a "Co-Authored-By" trailer to the commit message.

---

### Task 4: Reference script + manual capture verification

**Files:** none required, but reuse the existing `data/scripts/ranger_capture_test.inc` (already wired into the build from a prior task) as the manual-trigger path.

- [ ] **Step 1: Rebuild and confirm the full test suite still passes**

Run: `make clean && make -j$(nproc)` then `make TESTS="RangerCapture" check`.
Expected: clean build, 8/8 tests pass.

- [ ] **Step 2: Manual headless-capture verification**

This is not automatable by the implementer working through this plan in isolation — it requires the same `mgba-headless` + Lua-scripted-input capture process used earlier in this project's history (boot a save, reach the debug menu via R+START, repoint a `Debug_EventScript_Script_N` scratch slot at `RangerCaptureTest_EventScript_Start`, run it, screenshot the `RSTATE_PLAYING` state). Report back with:
- Whether the ring is visible at all (not hidden behind the target sprite, not clipped off-screen).
- Whether it visibly shrinks as loop progress increases and resets outward when a loop completes.
- Whether the target's overworld sprite renders as the correct species, right-side-up, not a garbled tile/palette mismatch.
- Whatever positioning/priority adjustments the capture shows are needed (the `152, 56` / `152, 88` placeholder coordinates from Tasks 1-2, and the OAM priority/subpriority values, are expected to need tuning here — this is normal, not a sign something is broken).

If adjustments are needed, make them, rebuild, and re-capture until the ring and sprite read correctly together. This is the acceptance gate for Phase 1 — there is no faster feedback loop for this class of visual change.

- [ ] **Step 3: Commit any tuning adjustments made during verification**

```bash
git add src/ranger_capture.c
git commit -m "Tune capture ring/target sprite positioning after headless-capture verification"
```

(Skip this step if Step 2 required no changes.) Do NOT add a "Co-Authored-By" trailer to the commit message.
