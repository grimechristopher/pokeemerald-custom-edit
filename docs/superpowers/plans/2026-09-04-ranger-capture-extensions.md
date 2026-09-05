# Ranger Capture Styler Extensions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the existing `src/ranger_capture.c` Ranger Capture Styler minigame with attack/obstacle notes, level-based difficulty scaling, and a standalone scripted-encounter trigger (`setstylercapture`/`dostylercapture`) that runs the minigame with no battle involved at all.

**Architecture:** Extract the difficulty math into a pure, unit-testable function (`ComputeRangerCaptureDifficulty`) driven by a small `struct RangerCaptureParams`, so the existing battle-item entry point and a new standalone entry point can both drive the same rhythm-game state machine. Add a note `kind` (normal/attack) to the existing note system. Add two new script commands mirroring `setwildbattle`/`dowildbattle` exactly.

**Tech Stack:** C (arm-none-eabi via devkitARM), this project's `TEST()` unit test framework (`test/test.h`), the existing script command / `special` plumbing (`data/script_cmd_table.inc`, `data/specials.inc`).

---

## Spec coverage map

- Shared `struct RangerCaptureParams` / pure difficulty extraction → Tasks 1-2
- Attack/obstacle notes → Task 3
- Level scaling → Task 1 (part of `ComputeRangerCaptureDifficulty`)
- Standalone scripted trigger (`setstylercapture`/`dostylercapture`, `GetStylerCaptureOutcome`, give-mon-to-player handoff) → Tasks 4-5
- Reference script + manual verification → Task 6
- Full build/test pass → Task 7

---

### Task 1: Pure difficulty function + unit tests

**Files:**
- Create: `include/constants/ranger_capture.h`
- Modify: `include/ranger_capture.h`
- Modify: `src/ranger_capture.c`
- Create: `test/ranger_capture.c`

This task only *adds* a new pure function alongside the existing `CalculateDifficulty` — it does not touch `CalculateDifficulty` or any call site yet, so the existing battle-item behavior cannot regress from this task.

- [ ] **Step 1: Add the result-code constants file**

Create `include/constants/ranger_capture.h`:

```c
#ifndef GUARD_CONSTANTS_RANGER_CAPTURE_H
#define GUARD_CONSTANTS_RANGER_CAPTURE_H

#define STYLER_RESULT_FAILED 0
#define STYLER_RESULT_CAUGHT 1

#endif // GUARD_CONSTANTS_RANGER_CAPTURE_H
```

- [ ] **Step 2: Declare the params/difficulty structs and the pure function in the header**

In `include/ranger_capture.h`, add after the existing `#define RANGER_CAPTURE_FAIL 3` block and before `extern u8 gRangerCaptureState;`:

```c
struct RangerCaptureParams
{
    u32 catchRate;
    u8  level;
    bool8 isIncapacitated; // asleep or frozen - eases difficulty
    bool8 isLowHp;          // under 25% max HP - eases difficulty
};

struct RangerDifficulty
{
    u8 loopsNeeded;
    u8 noteSpeed;
    u8 maxMisses;
    u8 attackNoteChance; // percent chance a spawned note is an attack note
};

struct RangerDifficulty ComputeRangerCaptureDifficulty(struct RangerCaptureParams params);
```

- [ ] **Step 3: Write the failing tests**

Create `test/ranger_capture.c`:

```c
#include "global.h"
#include "test/test.h"
#include "ranger_capture.h"

static struct RangerCaptureParams BaseParams(u32 catchRate)
{
    struct RangerCaptureParams params = {0};
    params.catchRate = catchRate;
    params.level = 5;
    return params;
}

TEST("RangerCapture: high catch rate is the easiest tier") {
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(BaseParams(150));
    EXPECT_EQ(diff.loopsNeeded, 3);
    EXPECT_EQ(diff.noteSpeed, 6);
    EXPECT_EQ(diff.maxMisses, 5);
    EXPECT_EQ(diff.attackNoteChance, 10);
}

TEST("RangerCapture: low catch rate is the hardest tier") {
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(BaseParams(10));
    EXPECT_EQ(diff.loopsNeeded, 6);
    EXPECT_EQ(diff.noteSpeed, 3);
    EXPECT_EQ(diff.maxMisses, 2);
    EXPECT_EQ(diff.attackNoteChance, 40);
}

TEST("RangerCapture: level 50+ speeds up notes by one") {
    struct RangerCaptureParams params = BaseParams(150);
    params.level = 50;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 5);
}

TEST("RangerCapture: level 80+ speeds up notes by two") {
    struct RangerCaptureParams params = BaseParams(150);
    params.level = 80;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 4);
}

TEST("RangerCapture: level speedup never drops speed below 3") {
    struct RangerCaptureParams params = BaseParams(10); // already at noteSpeed 3
    params.level = 80;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 3);
}

TEST("RangerCapture: incapacitated target eases note speed by one") {
    struct RangerCaptureParams params = BaseParams(150);
    params.isIncapacitated = TRUE;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 7);
}

TEST("RangerCapture: low HP eases note speed by one") {
    struct RangerCaptureParams params = BaseParams(150);
    params.isLowHp = TRUE;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 7);
}

TEST("RangerCapture: incapacitated and low HP eases stack") {
    struct RangerCaptureParams params = BaseParams(150);
    params.isIncapacitated = TRUE;
    params.isLowHp = TRUE;
    struct RangerDifficulty diff = ComputeRangerCaptureDifficulty(params);
    EXPECT_EQ(diff.noteSpeed, 8);
}
```

- [ ] **Step 4: Run the tests and confirm they fail to compile**

Run: `make TESTS="RangerCapture" check`
Expected: FAIL — `ComputeRangerCaptureDifficulty` is declared but not defined (linker error), since Step 5 hasn't happened yet.

- [ ] **Step 5: Implement `ComputeRangerCaptureDifficulty`**

In `src/ranger_capture.c`, add this new function directly above the existing `CalculateDifficulty` (leave `CalculateDifficulty` itself untouched for now):

```c
struct RangerDifficulty ComputeRangerCaptureDifficulty(struct RangerCaptureParams params)
{
    struct RangerDifficulty diff;

    if (params.catchRate >= 150)
    {
        diff.loopsNeeded = 3;
        diff.noteSpeed = 6;
        diff.maxMisses = 5;
        diff.attackNoteChance = 10;
    }
    else if (params.catchRate >= 100)
    {
        diff.loopsNeeded = 4;
        diff.noteSpeed = 5;
        diff.maxMisses = 4;
        diff.attackNoteChance = 20;
    }
    else if (params.catchRate >= 45)
    {
        diff.loopsNeeded = 5;
        diff.noteSpeed = 4;
        diff.maxMisses = 3;
        diff.attackNoteChance = 30;
    }
    else
    {
        diff.loopsNeeded = 6;
        diff.noteSpeed = 3;
        diff.maxMisses = 2;
        diff.attackNoteChance = 40;
    }

    // Higher-level targets push the tempo up, same direction a lower catch rate does.
    if (params.level >= 50 && diff.noteSpeed > 3)
        diff.noteSpeed--;
    if (params.level >= 80 && diff.noteSpeed > 3)
        diff.noteSpeed--;

    // Easier if asleep/frozen
    if (params.isIncapacitated)
        diff.noteSpeed++;

    // Easier if low HP
    if (params.isLowHp)
        diff.noteSpeed++;

    return diff;
}
```

Also add `#include "constants/ranger_capture.h"` to the top of `src/ranger_capture.c`, alongside the existing `#include "ranger_capture.h"`, so it's available before Task 4 needs `STYLER_RESULT_*`.

- [ ] **Step 6: Run the tests and confirm they pass**

Run: `make TESTS="RangerCapture" check`
Expected: PASS (8 tests)

- [ ] **Step 7: Commit**

```bash
git add include/constants/ranger_capture.h include/ranger_capture.h src/ranger_capture.c test/ranger_capture.c
git commit -m "Add pure difficulty calculation for Ranger Capture with level scaling"
```

---

### Task 2: Wire the battle entry point through the pure function (no behavior change)

**Files:**
- Modify: `src/ranger_capture.c`

This task replaces `CalculateDifficulty`'s body and its one call site (`DoInit`) to route through `ComputeRangerCaptureDifficulty`, and adds the `attackNoteChance` field the struct needs for Task 3. For the existing battle-item path this must reproduce today's exact numbers — Task 1's tests already pin the tiering math, so this step is pure wiring.

- [ ] **Step 1: Add `attackNoteChance` to `struct RangerCapture`**

In `src/ranger_capture.c`, add a field to the existing struct:

```c
struct RangerCapture {
    u8  rstate;
    u8  loopsNeeded;
    u8  loopsCompleted;
    u8  maxMisses;
    u8  missCount;
    u8  noteSpeed;
    u8  attackNoteChance;
    u8  moveTimer;
    u16 countdownTimer;
    s16 loopProgress;
    u8  feedbackTimer;
    u16 animTimer;
    u16 noteSpawnTimer;
    u8  notesSpawnedThisLoop;
    u16 tilemapBuffer[32 * 32];
    struct RangerNote notes[MAX_NOTES];
};
```

- [ ] **Step 2: Replace `CalculateDifficulty`'s body**

Replace the existing `CalculateDifficulty` function (the one reading `gBattleMons[gBattlerTarget]` directly) with:

```c
static void CalculateDifficulty(void)
{
    struct RangerCaptureParams params = {0};
    struct RangerDifficulty diff;

    params.catchRate = gSpeciesInfo[gBattleMons[gBattlerTarget].species].catchRate;
    params.level = gBattleMons[gBattlerTarget].level;
    params.isIncapacitated = (gBattleMons[gBattlerTarget].status1 & STATUS1_INCAPACITATED) != 0;
    params.isLowHp = gBattleMons[gBattlerTarget].hp * 4 < gBattleMons[gBattlerTarget].maxHP;

    diff = ComputeRangerCaptureDifficulty(params);
    sRanger->loopsNeeded = diff.loopsNeeded;
    sRanger->noteSpeed = diff.noteSpeed;
    sRanger->maxMisses = diff.maxMisses;
    sRanger->attackNoteChance = diff.attackNoteChance;
}
```

Note this reproduces the exact same tiering, level has no effect on this path today only because no existing caller passed a level before — now it does, which is an intentional part of this feature (a wild target's actual level now affects difficulty, whereas before level was ignored entirely).

- [ ] **Step 3: Build and confirm no compile errors**

Run: `make -j$(nproc)`
Expected: builds successfully, no errors.

- [ ] **Step 4: Re-run the Task 1 tests to confirm nothing broke**

Run: `make TESTS="RangerCapture" check`
Expected: PASS (still 8 tests)

- [ ] **Step 5: Commit**

```bash
git add src/ranger_capture.c
git commit -m "Route the battle-item Ranger Capture path through the shared difficulty function"
```

---

### Task 3: Attack/obstacle notes

**Files:**
- Modify: `src/ranger_capture.c`

- [ ] **Step 1: Add the note kind constants, tile, and palette color**

Add near the existing `NOTE_INACTIVE`/`NOTE_ACTIVE` defines:

```c
// Note kinds
#define NOTE_KIND_NORMAL 0
#define NOTE_KIND_ATTACK 1
```

Add a new tile index (bump `TILE_COUNT` from 10 to 11):

```c
#define TILE_METER_OFF 9
#define TILE_NOTE_ATTACK 10
#define TILE_COUNT     11
```

Add a new palette color index and its RGB entry, using the currently-unused slot 11:

```c
#define COL_METER_OFF 10
#define COL_ATTACK    11
```

In `sRangerBgPal`, change index 11 from `RGB(0, 0, 0), // 11-15: unused` to:

```c
RGB(20, 0, 20),   // 11: COL_ATTACK
```

In `sRangerBgTiles`, add:

```c
[TILE_NOTE_ATTACK] = SOLID_TILE(COL_ATTACK),
```

- [ ] **Step 2: Add `kind` to `struct RangerNote` and update `SpawnNote`/`GetNoteTile`**

```c
struct RangerNote {
    u8  lane;
    s16 tileCol;
    u8  state;
    u8  kind;
};
```

```c
static void SpawnNote(u8 lane, u8 kind)
{
    u32 i;
    for (i = 0; i < MAX_NOTES; i++)
    {
        if (sRanger->notes[i].state == NOTE_INACTIVE)
        {
            sRanger->notes[i].lane    = lane;
            sRanger->notes[i].tileCol = LANE_START_COL;
            sRanger->notes[i].state   = NOTE_ACTIVE;
            sRanger->notes[i].kind    = kind;
            return;
        }
    }
}

static u8 GetNoteTile(u8 lane, u8 kind)
{
    if (kind == NOTE_KIND_ATTACK)
        return TILE_NOTE_ATTACK;

    switch (lane)
    {
    case LANE_UP:    return TILE_NOTE_UP;
    case LANE_RIGHT: return TILE_NOTE_RT;
    case LANE_DOWN:  return TILE_NOTE_DN;
    default:         return TILE_NOTE_LT;
    }
}
```

Add `#include "random.h"` to the top of `src/ranger_capture.c` (needed for `Random()` in the next step).

- [ ] **Step 3: Roll a note kind on spawn, in `UpdateNotes`**

Change the spawn block inside `UpdateNotes`:

```c
if (sRanger->notesSpawnedThisLoop < NOTES_PER_LOOP_TOTAL)
{
    u8 lane = sRanger->notesSpawnedThisLoop % LANE_COUNT;
    u8 kind = (Random() % 100 < sRanger->attackNoteChance) ? NOTE_KIND_ATTACK : NOTE_KIND_NORMAL;
    SpawnNote(lane, kind);
    sRanger->notesSpawnedThisLoop++;
}
```

- [ ] **Step 4: Update the two `GetNoteTile` call sites in `UpdateNotes`**

The redraw-at-new-position call:

```c
u8 noteTile = GetNoteTile(sRanger->notes[i].lane, sRanger->notes[i].kind);
SetTile(col, row,     noteTile);
SetTile(col, row + 1, noteTile);
```

- [ ] **Step 5: Branch the end-of-lane handling on note kind, in `UpdateNotes`**

Replace the `if (col > LANE_END_COL)` block:

```c
if (col > LANE_END_COL)
{
    sRanger->notes[i].state = NOTE_INACTIVE;

    if (sRanger->notes[i].kind == NOTE_KIND_ATTACK)
    {
        // Letting an attack note through is correct play - small reward, no penalty.
        sRanger->loopProgress += 3;
        if (sRanger->loopProgress > LOOP_PROGRESS_MAX)
            sRanger->loopProgress = LOOP_PROGRESS_MAX;
        UpdateLoopMeter();
    }
    else
    {
        // Missed
        sRanger->missCount++;
        sRanger->loopProgress -= 15;
        if (sRanger->loopProgress < 0)
            sRanger->loopProgress = 0;
        PrintFeedback(HIT_MISS);
        sRanger->feedbackTimer = FEEDBACK_DURATION;
        UpdateLoopMeter();
        PlaySE(SE_BALL_BOUNCE_4);
    }
}
```

- [ ] **Step 6: Branch `HandleInput` on note kind**

Replace the hit-result section of `HandleInput` (from `u8 hitResult;` through the `PrintFeedback(hitResult);` call) with:

```c
u8 hitResult;
bool8 consumeNote = FALSE;

if (bestIdx < MAX_NOTES && sRanger->notes[bestIdx].kind == NOTE_KIND_ATTACK && bestDelta <= 2)
{
    // Pressing into the target's counterattack is backwards - that's on you.
    hitResult = HIT_MISS;
    consumeNote = TRUE;
    sRanger->missCount++;
    sRanger->loopProgress -= 25;
    if (sRanger->loopProgress < 0)
        sRanger->loopProgress = 0;
    PlaySE(SE_BALL_BOUNCE_4);
}
else if (bestDelta == 0)
{
    hitResult = HIT_PERFECT;
    consumeNote = TRUE;
    sRanger->loopProgress += 8;
    PlaySE(SE_SUCCESS);
}
else if (bestDelta == 1)
{
    hitResult = HIT_GOOD;
    consumeNote = TRUE;
    sRanger->loopProgress += 5;
    PlaySE(SE_SELECT);
}
else if (bestDelta == 2)
{
    hitResult = HIT_OK;
    consumeNote = TRUE;
    sRanger->loopProgress += 2;
    PlaySE(SE_SELECT);
}
else
{
    // Miss - no nearby note
    hitResult = HIT_MISS;
    sRanger->missCount++;
    sRanger->loopProgress -= 15;
    if (sRanger->loopProgress < 0)
        sRanger->loopProgress = 0;
    PlaySE(SE_BALL_BOUNCE_4);
}

// Consume the hit note
if (consumeNote && bestIdx < MAX_NOTES)
{
    u8  row = GetLaneRow(pressedLane);
    s16 col = sRanger->notes[bestIdx].tileCol;
    sRanger->notes[bestIdx].state = NOTE_INACTIVE;
    if (col >= LANE_START_COL && col <= LANE_END_COL)
    {
        SetTile(col, row,     LaneRestoreTile(col));
        SetTile(col, row + 1, LaneRestoreTile(col));
    }
}

if (sRanger->loopProgress > LOOP_PROGRESS_MAX)
    sRanger->loopProgress = LOOP_PROGRESS_MAX;

PrintFeedback(hitResult);
sRanger->feedbackTimer = FEEDBACK_DURATION;
UpdateLoopMeter();
```

This replaces the entire remainder of the function body in one go — the old `if (hitResult != HIT_MISS && bestIdx < MAX_NOTES)` consume block, the `loopProgress` clamp, and the `PrintFeedback` call are all included in the block being replaced above, so there's nothing left over to delete separately.

- [ ] **Step 7: Build**

Run: `make -j$(nproc)`
Expected: builds successfully.

- [ ] **Step 8: Manual verification in mGBA**

Load the ROM, get a `ITEM_CAPTURE_STYLER` (debug menu → Util → Give Item, or `Bag_AddItemAndShow`-style debug helper), start a wild battle, throw it. Confirm:
- A visibly different-colored note (purple, `COL_ATTACK`) appears in lanes alongside the normal colored notes.
- Pressing the note's direction while an attack note is in the hit zone shows "MISS!" and costs more loop progress than a normal miss.
- Letting an attack note scroll off the end without pressing does *not* count as a miss (no "MISS!" text, no miss-count increment).

- [ ] **Step 9: Commit**

```bash
git add src/ranger_capture.c
git commit -m "Add attack/obstacle notes to Ranger Capture"
```

---

### Task 4: Standalone entry point and give-mon-to-player handoff

**Files:**
- Modify: `include/ranger_capture.h`
- Modify: `src/ranger_capture.c`

- [ ] **Step 1: Add result-mode constants and new declarations to the header**

In `include/ranger_capture.h`, add alongside the existing `RANGER_CAPTURE_*` defines:

```c
// Which caller started the minigame, and therefore how DoExit hands off its result
#define RANGER_RESULT_MODE_BATTLE   0
#define RANGER_RESULT_MODE_SCRIPTED 1
```

Add `#include "constants/species.h"` at the top of the file, alongside the existing `#include "main.h"` (needed for `enum Species` in the declaration below). Then add these declarations near `void RangerCapture_Init(void);`:

```c
void RangerCapture_InitStandalone(void);
void SetStagedStylerCaptureMon(enum Species species, u8 level);
u8 GetStylerCaptureOutcome(void);
```

- [ ] **Step 2: Add `resultMode` to `struct RangerCapture` and the new file-scope statics**

```c
struct RangerCapture {
    u8  rstate;
    u8  resultMode;
    u8  loopsNeeded;
    ...
```

Near the existing `EWRAM_DATA static struct RangerCapture *sRanger = NULL;`, add:

```c
static enum Species sStagedStylerSpecies;
static u8 sStagedStylerLevel;
static u8 sStylerCaptureOutcome;
```

- [ ] **Step 3: Add the includes the standalone path needs**

Add to the top of `src/ranger_capture.c`:

```c
#include "overworld.h"
#include "ow_abilities.h"
```

(`overworld.h` for `CB2_ReturnToFieldContinueScriptPlayMapMusic`; `ow_abilities.h` for `GetSynchronizedGender`/`GetSynchronizedNature`/`STATIC_WILDMON_ORIGIN`.)

- [ ] **Step 4: Split `RangerCapture_Init` into a common initializer plus two thin entry points**

Replace the existing `void RangerCapture_Init(void)` function with:

```c
static void RangerCapture_InitCommon(u8 resultMode)
{
    SetVBlankCallback(NULL);
    ResetTasks();
    ResetSpriteData();
    ResetPaletteFade();
    FreeAllSpritePalettes();
    ScanlineEffect_Stop();

    sRanger = AllocZeroed(sizeof(struct RangerCapture));
    sRanger->resultMode = resultMode;

    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);

    sRanger->rstate = RSTATE_INIT;
    CreateTask(Task_RangerCapture, 0);

    SetVBlankCallback(RangerCapture_VBlankCB);
    SetMainCallback2(RangerCapture_MainCB);
}

void RangerCapture_Init(void)
{
    RangerCapture_InitCommon(RANGER_RESULT_MODE_BATTLE);
}

void RangerCapture_InitStandalone(void)
{
    RangerCapture_InitCommon(RANGER_RESULT_MODE_SCRIPTED);
}

void SetStagedStylerCaptureMon(enum Species species, u8 level)
{
    sStagedStylerSpecies = species;
    sStagedStylerLevel = level;
}

u8 GetStylerCaptureOutcome(void)
{
    return sStylerCaptureOutcome;
}
```

- [ ] **Step 5: Make `DoInit` build params for either mode**

Replace `DoInit`:

```c
static void DoInit(void)
{
    CalculateDifficulty();
    sRanger->rstate = RSTATE_SETUP_GFX;
}
```

with:

```c
static void DoInit(void)
{
    CalculateDifficultyForMode();
    sRanger->rstate = RSTATE_SETUP_GFX;
}
```

And replace the battle-only `CalculateDifficulty` from Task 2 with a mode-aware version (rename it `CalculateDifficultyForMode` to make the branch explicit):

```c
static void CalculateDifficultyForMode(void)
{
    struct RangerCaptureParams params = {0};
    struct RangerDifficulty diff;

    if (sRanger->resultMode == RANGER_RESULT_MODE_SCRIPTED)
    {
        params.catchRate = gSpeciesInfo[sStagedStylerSpecies].catchRate;
        params.level = sStagedStylerLevel;
        // No live battle mon to read status/HP from for a scripted encounter.
        params.isIncapacitated = FALSE;
        params.isLowHp = FALSE;
    }
    else
    {
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

- [ ] **Step 6: Branch `DoExit` on `resultMode`**

Replace `DoExit`:

```c
static void DoExit(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    FreeAllWindowBuffers();
    HideBg(0);
    ResetBgsAndClearDma3BusyFlags(0);

    if (sRanger->resultMode == RANGER_RESULT_MODE_SCRIPTED)
    {
        if (gRangerCaptureState == RANGER_CAPTURE_SUCCESS)
        {
            struct Pokemon mon;
            u32 personality = GetMonPersonality(sStagedStylerSpecies,
                GetSynchronizedGender(STATIC_WILDMON_ORIGIN, sStagedStylerSpecies),
                GetSynchronizedNature(STATIC_WILDMON_ORIGIN, sStagedStylerSpecies),
                RANDOM_UNOWN_LETTER);

            CreateMonWithIVs(&mon, sStagedStylerSpecies, sStagedStylerLevel, personality, OTID_STRUCT_PLAYER_ID, USE_RANDOM_IVS);
            GiveMonInitialMoveset(&mon);
            GiveScriptedMonToPlayer(&mon, PARTY_SIZE);
            sStylerCaptureOutcome = STYLER_RESULT_CAUGHT;
        }
        else
        {
            sStylerCaptureOutcome = STYLER_RESULT_FAILED;
        }

        Free(sRanger);
        sRanger = NULL;
        DestroyTask(taskId);
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        return;
    }

    Free(sRanger);
    sRanger = NULL;

    DestroyTask(taskId);
    SetMainCallback2(gRangerCapture_ReturnCallback);
}
```

- [ ] **Step 7: Build**

Run: `make -j$(nproc)`
Expected: builds successfully. (`gRangerCaptureState` is reset to `RANGER_CAPTURE_IDLE` by the existing battle-side code after every battle-mode run; the scripted path doesn't need to reset it before starting since `RangerCapture_InitCommon` doesn't read it, only `DoPlaying` writes it on success/fail.)

- [ ] **Step 8: Re-run Task 1's tests**

Run: `make TESTS="RangerCapture" check`
Expected: PASS (still 8 tests — this task didn't touch `ComputeRangerCaptureDifficulty`).

- [ ] **Step 9: Commit**

```bash
git add include/ranger_capture.h src/ranger_capture.c
git commit -m "Add a standalone (non-battle) entry point to Ranger Capture"
```

---

### Task 5: Script commands

**Files:**
- Modify: `src/scrcmd.c`
- Modify: `data/script_cmd_table.inc`
- Modify: `data/specials.inc`

- [ ] **Step 1: Add the include**

In `src/scrcmd.c`, add between the existing `#include "random.h"` and `#include "rotating_tile_puzzle.h"`:

```c
#include "ranger_capture.h"
```

- [ ] **Step 2: Add the two script command functions**

In `src/scrcmd.c`, add immediately after `ScrCmd_dowildbattle` (defined right after `ScrCmd_setwildbattle`):

```c
bool8 ScrCmd_setstylercapture(struct ScriptContext *ctx)
{
    enum Species species = ScriptReadHalfword(ctx);
    u8 level = ScriptReadByte(ctx);

    Script_RequestEffects(SCREFF_V1);

    SetStagedStylerCaptureMon(species, level);
    return FALSE;
}

bool8 ScrCmd_dostylercapture(struct ScriptContext *ctx)
{
    Script_RequestEffects(SCREFF_V1 | SCREFF_HARDWARE);

    LockPlayerFieldControls();
    SetMainCallback2(RangerCapture_InitStandalone);
    ScriptContext_Stop();

    return TRUE;
}
```

- [ ] **Step 3: Register the two opcodes**

In `data/script_cmd_table.inc`, add before the `.if ALLOCATE_SCRIPT_CMD_TABLE` block at the end of the file:

```
	script_cmd_table_entry SCR_OP_SETSTYLERCAPTURE               ScrCmd_setstylercapture,            requests_effects=1  @ 0xe7
	script_cmd_table_entry SCR_OP_DOSTYLERCAPTURE                ScrCmd_dostylercapture,              requests_effects=1  @ 0xe8
```

- [ ] **Step 4: Register the result-reading special**

In `data/specials.inc`, add a line right after the existing `def_special GetBattleOutcome` (around line 204):

```
	def_special GetStylerCaptureOutcome
```

- [ ] **Step 5: Build**

Run: `make -j$(nproc)`
Expected: builds successfully. This regenerates `include/constants/script_commands.h` with `SCR_OP_SETSTYLERCAPTURE`/`SCR_OP_DOSTYLERCAPTURE` automatically — don't hand-edit that file.

- [ ] **Step 6: Commit**

```bash
git add src/scrcmd.c data/script_cmd_table.inc data/specials.inc
git commit -m "Add setstylercapture/dostylercapture script commands"
```

---

### Task 6: Reference script and manual verification

**Files:**
- Create: `data/scripts/ranger_capture_test.inc`
- Modify: `data/event_scripts.s`

- [ ] **Step 1: Write the reference script**

Create `data/scripts/ranger_capture_test.inc`:

```
RangerCaptureTest_EventScript_Start::
	setstylercapture SPECIES_BEEDRILL, 20
	dostylercapture
	specialvar VAR_RESULT, GetStylerCaptureOutcome
	goto_if_eq VAR_RESULT, STYLER_RESULT_CAUGHT, RangerCaptureTest_EventScript_Caught
	msgbox RangerCaptureTest_Text_BrokeFree, MSGBOX_DEFAULT
	end

RangerCaptureTest_EventScript_Caught::
	msgbox RangerCaptureTest_Text_Caught, MSGBOX_DEFAULT
	end

RangerCaptureTest_Text_Caught:
	.string "You caught it with the styler!$"

RangerCaptureTest_Text_BrokeFree:
	.string "It broke free of the styler.$"
```

- [ ] **Step 2: Wire it into the build**

In `data/event_scripts.s`, add a new line right after the existing `.include "data/scripts/abnormal_weather.inc"`:

```
	.include "data/scripts/ranger_capture_test.inc"
```

- [ ] **Step 3: Build**

Run: `make -j$(nproc)`
Expected: builds successfully.

- [ ] **Step 4: Manually trigger and verify both entry points in mGBA**

Standalone path — this script isn't attached to any map object yet, so trigger it directly: in the debug menu (R+START by default), find the "Call script" / debug script-execution option and point it at `RangerCaptureTest_EventScript_Start` (or temporarily attach it to any NPC's script in porymap and talk to them). Confirm:
- The styler minigame launches directly — no battle transition, no wild Pokémon cry, no battle menu at any point.
- Beating it (fill the loop-progress meter the required number of times before running out of misses) shows "You caught it with the styler!" and the Beedrill appears in your party (or PC if full) — check the party screen or PC to confirm species/level 20.
- Failing it (run out of misses) shows "It broke free of the styler." and nothing was added to your party.

Battle-item path — confirm it's unaffected: get an `ITEM_CAPTURE_STYLER`, start any wild battle, throw it. Confirm the DDR minigame still launches exactly as before, and a caught Pokémon still ends up in the party/PC through the normal battle catch flow.

- [ ] **Step 5: Commit**

```bash
git add data/scripts/ranger_capture_test.inc data/event_scripts.s
git commit -m "Add a reference/smoke-test script for the standalone Ranger Capture trigger"
```

---

### Task 7: Full verification pass

**Files:** none (verification only)

- [ ] **Step 1: Full clean build**

Run: `make clean && make -j$(nproc)`
Expected: builds successfully with no new warnings introduced by this feature's files.

- [ ] **Step 2: Full RangerCapture test run**

Run: `make TESTS="RangerCapture" check`
Expected: PASS (8 tests from Task 1).

- [ ] **Step 3: Full test suite sanity check**

Run: `make check -j`
Expected: PASS — confirms the `struct RangerCapture` field addition, new includes, and script command table changes didn't break anything elsewhere (e.g. no opcode collision, no header include-order issue).

- [ ] **Step 4: Re-run the manual mGBA checks from Task 3 Step 8 and Task 6 Step 4**

Confirm once more, together, in a single play session: attack notes in the battle-item path, and the full standalone success/failure flow. This is the first point both features are exercised in the same build.

- [ ] **Step 5: Final commit (if anything changed)**

```bash
git add -A
git commit -m "Verify Ranger Capture Styler extensions build and pass tests"
```

(Skip this step if Steps 1-4 required no code changes — an empty verification pass needs no commit.)
