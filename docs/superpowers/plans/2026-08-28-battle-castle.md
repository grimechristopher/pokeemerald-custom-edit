# Battle Castle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Sinnoh's Battle Castle — a Castle Points (CP) economy wrapped around ordinary Trainer battles — playable from the debug menu, per `docs/superpowers/specs/2026-08-28-sinnoh-battle-frontier-design.md`.

**Architecture:** No new `BATTLE_TYPE_*` flag or battle-engine changes — the battle itself is an ordinary `BATTLE_TYPE_TRAINER` battle at level 50. Everything distinctive lives in a new facility-controller file (`battle_castle.c`/`.h`): a pure CP-earning formula, a pure CP-spending shop, and glue code that reads the party's post-battle state to compute CP earned, following the exact pattern `battle_dome.c`/`battle_pike.c` already use to wrap ordinary battles.

**Tech Stack:** C (GCC via devkitARM), the project's `TEST()` unit test macro (`make check`).

---

## Before you start

Read `docs/superpowers/specs/2026-08-28-sinnoh-battle-frontier-design.md` for full context. This plan implements only the Battle Castle section.

This plan depends on `IncrementDailyBattlePoints` being made non-static (done in Task 4 of `docs/superpowers/plans/2026-08-28-battle-arcade.md`). If the Arcade plan hasn't been executed yet, redo that one small change here first: remove `static` from `IncrementDailyBattlePoints` in `src/frontier_util.c` and add its prototype to `include/frontier_util.h`.

All new persistent fields go at the **tail** of `struct BattleFrontier` in `include/global.h` — this is a save-breaking change and must be called out as such in the final PR description.

---

### Task 1: CP-earning formula

**Files:**
- Create: `include/battle_castle.h`
- Create: `src/battle_castle.c`
- Create: `test/battle_castle.c`

- [ ] **Step 1: Write the failing test**

Create `test/battle_castle.c`:

```c
#include "global.h"
#include "test/test.h"
#include "battle_castle.h"

TEST("CalculateCastlePoints awards full per-mon bonuses for a clean sweep")
{
    struct CastleMonResult mons[MAX_FRONTIER_PARTY_SIZE] =
    {
        { .fainted = FALSE, .hpPercent = 100, .hasStatus = FALSE },
        { .fainted = FALSE, .hpPercent = 100, .hasStatus = FALSE },
        { .fainted = FALSE, .hpPercent = 100, .hasStatus = FALSE },
    };

    // Per mon: 3 (unfainted) + 3 (full HP) + 1 (no status) = 7. Three mons = 21.
    // 0 PP used = +8. 0 levels raised = +0. Total 29.
    EXPECT_EQ(CalculateCastlePoints(mons, 0, 0), 29);
}

TEST("CalculateCastlePoints reduces HP and PP tiers correctly")
{
    struct CastleMonResult mons[MAX_FRONTIER_PARTY_SIZE] =
    {
        { .fainted = TRUE,  .hpPercent = 0,  .hasStatus = FALSE },
        { .fainted = FALSE, .hpPercent = 60, .hasStatus = TRUE },
        { .fainted = FALSE, .hpPercent = 20, .hasStatus = FALSE },
    };

    // Mon 1 (fainted): 0 + 1 (hp<50 tier) + 1 (no status) = 2
    // Mon 2: 3 (unfainted) + 2 (hp>=50) + 0 (has status) = 5
    // Mon 3: 3 (unfainted) + 1 (hp<50) + 1 (no status) = 5
    // Subtotal 12. 12 PP used (tier 11-15) = +4. Total 16.
    EXPECT_EQ(CalculateCastlePoints(mons, 12, 0), 16);
}

TEST("CalculateCastlePoints adds 7 CP per 5 levels the opponent was raised, capped at 50")
{
    struct CastleMonResult mons[MAX_FRONTIER_PARTY_SIZE] =
    {
        { .fainted = FALSE, .hpPercent = 100, .hasStatus = FALSE },
        { .fainted = FALSE, .hpPercent = 100, .hasStatus = FALSE },
        { .fainted = FALSE, .hpPercent = 100, .hasStatus = FALSE },
    };

    // 29 (as above) + 3*7 (15 levels raised) = 50, exactly the cap
    EXPECT_EQ(CalculateCastlePoints(mons, 0, 15), 50);
    // Any more would exceed 50 and must clamp
    EXPECT_EQ(CalculateCastlePoints(mons, 0, 20), 50);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="CalculateCastlePoints" check`
Expected: FAIL to compile — `battle_castle.h` doesn't exist yet.

- [ ] **Step 3: Write the header**

Create `include/battle_castle.h`:

```c
#ifndef GUARD_BATTLE_CASTLE_H
#define GUARD_BATTLE_CASTLE_H

struct CastleMonResult
{
    bool8 fainted;
    u8 hpPercent; // 0-100
    bool8 hasStatus;
};

#define CASTLE_STARTING_CP     10
#define CASTLE_COST_SCOUT_SPECIES 1
#define CASTLE_COST_SCOUT_MOVES   5
#define CASTLE_COST_RAISE_OPPONENT_LEVEL_PER_5 3
#define CASTLE_COST_HEAL_HP    10
#define CASTLE_COST_HEAL_PP    8
#define CASTLE_COST_HEAL_FULL  12

u32 CalculateCastlePoints(const struct CastleMonResult mons[MAX_FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised);

#endif // GUARD_BATTLE_CASTLE_H
```

- [ ] **Step 4: Implement `CalculateCastlePoints`**

Create `src/battle_castle.c`:

```c
#include "global.h"
#include "battle_castle.h"

u32 CalculateCastlePoints(const struct CastleMonResult mons[MAX_FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised)
{
    u32 i;
    u32 points = 0;

    for (i = 0; i < MAX_FRONTIER_PARTY_SIZE; i++)
    {
        if (!mons[i].fainted)
            points += 3;

        if (mons[i].hpPercent >= 100)
            points += 3;
        else if (mons[i].hpPercent >= 50)
            points += 2;
        else
            points += 1;

        if (!mons[i].hasStatus)
            points += 1;
    }

    if (totalPPUsed <= 5)
        points += 8;
    else if (totalPPUsed <= 10)
        points += 6;
    else if (totalPPUsed <= 15)
        points += 4;

    // 7 CP bonus per 5 levels the opponent was raised. This is independent of
    // CASTLE_COST_RAISE_OPPONENT_LEVEL_PER_5, which is the separate CP *cost* charged
    // in the shop (Task 2) for raising the level in the first place.
    points += (opponentLevelsRaised / 5) * 7;

    if (points > 50)
        points = 50;

    return points;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `make TESTS="CalculateCastlePoints" check`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add include/battle_castle.h src/battle_castle.c test/battle_castle.c
git commit -m "Add Battle Castle CP-earning formula"
```

---

### Task 2: CP shop spending

**Files:**
- Modify: `include/battle_castle.h`
- Modify: `src/battle_castle.c`
- Modify: `test/battle_castle.c`

- [ ] **Step 1: Write the failing test**

Append to `test/battle_castle.c`:

```c
TEST("CastleShop_TrySpend deducts CP only when affordable")
{
    u32 cp = 10;

    EXPECT_TRUE(CastleShop_TrySpend(&cp, CASTLE_COST_SCOUT_SPECIES));
    EXPECT_EQ(cp, 9);

    EXPECT_TRUE(CastleShop_TrySpend(&cp, CASTLE_COST_HEAL_FULL));
    EXPECT_EQ(cp, 9 - CASTLE_COST_HEAL_FULL);
}

TEST("CastleShop_TrySpend refuses to go negative")
{
    u32 cp = 2;

    EXPECT_FALSE(CastleShop_TrySpend(&cp, CASTLE_COST_HEAL_FULL));
    EXPECT_EQ(cp, 2); // unchanged
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="CastleShop_TrySpend" check`
Expected: FAIL to compile — `CastleShop_TrySpend` not declared yet.

- [ ] **Step 3: Add the declaration and implementation**

In `include/battle_castle.h`, add:

```c
bool8 CastleShop_TrySpend(u32 *cp, u32 cost);
```

In `src/battle_castle.c`, add:

```c
bool8 CastleShop_TrySpend(u32 *cp, u32 cost)
{
    if (*cp < cost)
        return FALSE;

    *cp -= cost;
    return TRUE;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `make TESTS="CastleShop_TrySpend" check`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/battle_castle.h src/battle_castle.c test/battle_castle.c
git commit -m "Add Battle Castle CP shop spending"
```

---

### Task 3: Reading battle results into a CP calculation

**Files:**
- Modify: `include/battle_castle.h`
- Modify: `src/battle_castle.c`
- Modify: `test/battle_castle.c`

This task builds the glue that reads `gPlayerParty` after a real battle ends and turns it into the `CastleMonResult` array `CalculateCastlePoints` needs, plus total PP used across the party's moves.

- [ ] **Step 1: Write the failing test**

Append to `test/battle_castle.c`:

```c
#include "pokemon.h"

TEST("GetCastleMonResults reads fainted state, HP percent, and status from a party")
{
    struct CastleMonResult results[MAX_FRONTIER_PARTY_SIZE];
    struct Pokemon party[MAX_FRONTIER_PARTY_SIZE];
    u32 i;
    u32 hp;

    for (i = 0; i < MAX_FRONTIER_PARTY_SIZE; i++)
        CreateMon(&party[i], SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);

    hp = 0;
    SetMonData(&party[0], MON_DATA_HP, &hp); // faint mon 0

    GetCastleMonResults(party, results);

    EXPECT_TRUE(results[0].fainted);
    EXPECT_EQ(results[0].hpPercent, 0);
    EXPECT_FALSE(results[1].fainted);
    EXPECT_EQ(results[1].hpPercent, 100);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="GetCastleMonResults" check`
Expected: FAIL to compile — function not declared yet.

- [ ] **Step 3: Add the declaration and implementation**

In `include/battle_castle.h`, add:

```c
void GetCastleMonResults(const struct Pokemon party[MAX_FRONTIER_PARTY_SIZE], struct CastleMonResult results[MAX_FRONTIER_PARTY_SIZE]);
```

In `src/battle_castle.c`, add (needs `#include "pokemon.h"`):

```c
void GetCastleMonResults(const struct Pokemon party[MAX_FRONTIER_PARTY_SIZE], struct CastleMonResult results[MAX_FRONTIER_PARTY_SIZE])
{
    u32 i;

    for (i = 0; i < MAX_FRONTIER_PARTY_SIZE; i++)
    {
        u32 hp = GetMonData(&party[i], MON_DATA_HP, NULL);
        u32 maxHp = GetMonData(&party[i], MON_DATA_MAX_HP, NULL);

        results[i].fainted = (hp == 0);
        results[i].hpPercent = maxHp == 0 ? 0 : (hp * 100) / maxHp;
        results[i].hasStatus = (GetMonData(&party[i], MON_DATA_STATUS, NULL) != 0);
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `make TESTS="GetCastleMonResults" check`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/battle_castle.h src/battle_castle.c test/battle_castle.c
git commit -m "Add Battle Castle post-battle result reading"
```

---

### Task 4: Persisted streak fields and challenge state

**Files:**
- Modify: `include/global.h`
- Modify: `include/battle_castle.h`
- Modify: `src/battle_castle.c`

- [ ] **Step 1: Append persisted fields to `struct BattleFrontier`**

In `include/global.h`, add at the tail (after the fields added by the Arcade plan, or after `domePlayerPartyData` if Arcade hasn't run yet):

```c
    /*New*/ u16 castleWinStreaks[FRONTIER_LVL_MODE_COUNT];
    /*New*/ u16 castleRecordWinStreaks[FRONTIER_LVL_MODE_COUNT];
    /*New*/ u16 castlePrize;
```

- [ ] **Step 2: Add in-progress challenge state**

In `include/battle_castle.h`, add:

```c
void Castle_StartChallenge(void);
u32 Castle_GetCurrentCP(void);
bool8 Castle_TrySpendCP(u32 cost);
void Castle_ApplyBattleResult(bool8 won, const struct Pokemon party[MAX_FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised);
u16 Castle_GetWinStreak(u8 lvlMode);
```

In `src/battle_castle.c`, add:

```c
#include "event_data.h"
#include "constants/battle_frontier.h"

static EWRAM_DATA u32 sCurrentCP = 0;
static EWRAM_DATA u16 sCurrentStreak = 0;

void Castle_StartChallenge(void)
{
    sCurrentCP = CASTLE_STARTING_CP;
    sCurrentStreak = 0;
}

u32 Castle_GetCurrentCP(void)
{
    return sCurrentCP;
}

bool8 Castle_TrySpendCP(u32 cost)
{
    return CastleShop_TrySpend(&sCurrentCP, cost);
}

void Castle_ApplyBattleResult(bool8 won, const struct Pokemon party[MAX_FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised)
{
    struct CastleMonResult results[MAX_FRONTIER_PARTY_SIZE];

    if (!won)
    {
        sCurrentCP = CASTLE_STARTING_CP;
        sCurrentStreak = 0;
        return;
    }

    GetCastleMonResults(party, results);
    sCurrentCP += CalculateCastlePoints(results, totalPPUsed, opponentLevelsRaised);
    sCurrentStreak++;
    if (sCurrentStreak > gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_MODE_LEVEL_50])
        gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_MODE_LEVEL_50] = sCurrentStreak;
    if (sCurrentStreak > gSaveBlock2Ptr->frontier.castleRecordWinStreaks[FRONTIER_LVL_MODE_LEVEL_50])
        gSaveBlock2Ptr->frontier.castleRecordWinStreaks[FRONTIER_LVL_MODE_LEVEL_50] = sCurrentStreak;
}

u16 Castle_GetWinStreak(u8 lvlMode)
{
    return gSaveBlock2Ptr->frontier.castleWinStreaks[lvlMode];
}
```

`FRONTIER_LVL_MODE_LEVEL_50` should already exist as part of `FRONTIER_LVL_MODE_COUNT`'s enum in `include/constants/battle_frontier.h` — confirm its exact name with `grep -n "FRONTIER_LVL_MODE_LEVEL_50\|FRONTIER_LVL_MODE_OPEN" include/constants/battle_frontier.h` before using it; if the codebase spells it differently (e.g. `FRONTIER_LVL_MODE_50`), use that spelling instead throughout this task.

- [ ] **Step 3: Build to confirm no compile errors**

Run: `make -j$(nproc)`
Expected: builds cleanly.

- [ ] **Step 4: Commit**

```bash
git add include/global.h include/battle_castle.h src/battle_castle.c
git commit -m "Add Battle Castle streak persistence and challenge state"
```

---

### Task 5: Debug menu integration

**Files:**
- Modify: `src/debug.c`

This reuses the `sDebugMenu_Actions_BattleFrontier` submenu added in the Battle Arcade plan's Task 6. If that plan hasn't been executed yet, create the submenu here first following the same pattern shown there.

- [ ] **Step 1: Add the menu entry**

In `src/debug.c`, add to `sDebugMenu_Actions_BattleFrontier[]`:

```c
    { COMPOUND_STRING("Start Castle Battle"), DebugAction_BattleFrontier_CastleBattle },
```

- [ ] **Step 2: Implement the debug action**

For this debug-menu phase, the shop is skipped (no interactive CP spending UI yet — that's part of the later full-facility build) and PP-used/level-raised are read as 0/0, matching a player who spent nothing:

```c
#include "battle_castle.h"

static void DebugAction_BattleFrontier_CastleBattle(u8 taskId)
{
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateNPCTrainerPartyFromTrainer(gPlayerParty, &sDebugTrainers[DIFFICULTY_NORMAL][DEBUG_TRAINER_PLAYER], TRUE, BATTLE_TYPE_TRAINER);
    CreateNPCTrainerPartyFromTrainer(gEnemyParty, GetDebugAiTrainer(), FALSE, BATTLE_TYPE_TRAINER);

    Castle_StartChallenge();

    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gDebugAIFlags = sDebugTrainers[DIFFICULTY_NORMAL][DEBUG_TRAINER_AI].aiFlags;
    gIsDebugBattle = TRUE;
    gBattleEnvironment = BattleSetup_GetEnvironmentId();
    CalculateEnemyPartyCount();
    BattleSetup_StartTrainerBattle_Debug();
    Debug_DestroyMenu_Full(taskId);
}
```

Recording the result (`Castle_ApplyBattleResult`) needs to run once the battle actually ends and control returns to the overworld, not inside this function (which only launches the battle). Wire it into `CB2_EndTrainerBattle`'s completion path:

- [ ] **Step 3: Call `Castle_ApplyBattleResult` when a debug Castle battle ends**

In `src/battle_setup.c`, find `CB2_EndTrainerBattle` (used by all trainer battles, debug ones included). Add a check right after the outcome is known (look for where `gBattleOutcome` is read, e.g. near `if (gBattleOutcome == B_OUTCOME_WON)`), guarded by a new flag so it only fires for Castle debug battles:

```c
extern bool8 gIsDebugCastleBattle; // declared in battle_castle.h, defined in battle_castle.c

if (gIsDebugCastleBattle)
{
    Castle_ApplyBattleResult(gBattleOutcome == B_OUTCOME_WON, gPlayerParty, 0, 0);
    gIsDebugCastleBattle = FALSE;
}
```

Add `EWRAM_DATA bool8 gIsDebugCastleBattle = FALSE;` to `src/battle_castle.c` and its `extern` declaration to `include/battle_castle.h`, and set `gIsDebugCastleBattle = TRUE;` in `DebugAction_BattleFrontier_CastleBattle` from Step 2, right before `BattleSetup_StartTrainerBattle_Debug();`.

- [ ] **Step 4: Build the ROM**

Run: `make -j$(nproc)`
Expected: builds cleanly.

- [ ] **Step 5: Manual verification**

Run the ROM in mGBA, open the debug menu, choose **Battle Frontier… → Start Castle Battle**, win the battle, and confirm (via a breakpoint or by temporarily logging `Castle_GetCurrentCP()`) that CP increased by a value consistent with `CalculateCastlePoints`'s formula.

- [ ] **Step 6: Commit**

```bash
git add src/debug.c src/battle_setup.c include/battle_castle.h src/battle_castle.c
git commit -m "Wire Battle Castle into the debug menu"
```

---

## Plan self-review notes

- The interactive pre-battle shop UI (scout/level/heal menus) is explicitly deferred per the spec's non-goals — this plan implements the shop's *spending logic* (`CastleShop_TrySpend`, fully tested) but the debug entry doesn't present a menu for it yet, matching "mechanics only, debug-menu testable" scope. Hooking the shop UI up to real player choices is part of the later full-facility phase.
- PP-used tracking (for the CP formula's PP-efficiency bonus) is passed as `0` from the debug entry rather than computed from the actual battle, since that requires diffing move PP before/after battle — flagged here rather than silently faked: a future task should capture `gPlayerParty` PP values before `BattleSetup_StartTrainerBattle_Debug()` and diff them against the post-battle values in the `CB2_EndTrainerBattle` hook.
