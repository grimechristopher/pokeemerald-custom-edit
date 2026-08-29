# Battle Hall Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Sinnoh's Battle Hall — single-Pokémon battles against a type-locked, rank-scaled opponent pool generated programmatically from base stat totals — playable from the debug menu, per `docs/superpowers/specs/2026-08-28-sinnoh-battle-frontier-design.md`.

**Architecture:** No new `BATTLE_TYPE_*` flag or battle-engine changes — the battle is an ordinary single `BATTLE_TYPE_TRAINER` battle. All new work is an opponent generator (`battle_hall.c`/`.h`) that buckets `gSpeciesInfo` by base stat total (BST) per type, filtered through the existing `gSpeciesInfo[species].isFrontierBanned` flag, plus per-type rank persistence.

**Tech Stack:** C (GCC via devkitARM), the project's `TEST()` unit test macro (`make check`).

---

## Before you start

Read `docs/superpowers/specs/2026-08-28-sinnoh-battle-frontier-design.md` for full context. This plan implements only the Battle Hall section.

All new persistent fields go at the **tail** of `struct BattleFrontier` in `include/global.h` — this is a save-breaking change and must be called out as such in the final PR description.

---

### Task 1: BST-to-rank bracket gating

**Files:**
- Create: `include/battle_hall.h`
- Create: `src/battle_hall.c`
- Create: `test/battle_hall.c`

- [ ] **Step 1: Write the failing test**

Create `test/battle_hall.c`:

```c
#include "global.h"
#include "test/test.h"
#include "battle_hall.h"

TEST("IsBstAvailableAtRank gates species by base stat total and rank")
{
    // BST < 340 is available from Rank 1
    EXPECT_TRUE(IsBstAvailableAtRank(200, 1));
    EXPECT_TRUE(IsBstAvailableAtRank(339, 1));

    // BST 340-439 only from Rank 3
    EXPECT_FALSE(IsBstAvailableAtRank(400, 1));
    EXPECT_FALSE(IsBstAvailableAtRank(400, 2));
    EXPECT_TRUE(IsBstAvailableAtRank(400, 3));

    // BST 440+ only from Rank 6
    EXPECT_FALSE(IsBstAvailableAtRank(450, 5));
    EXPECT_TRUE(IsBstAvailableAtRank(450, 6));
    EXPECT_TRUE(IsBstAvailableAtRank(720, 10)); // very high BST still gated the same way, no upper bound
}

TEST("GetSpeciesBST sums all six base stats")
{
    u32 bst = GetSpeciesBST(SPECIES_WOBBUFFET);
    u32 expected = GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_HP)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_ATK)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_DEF)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_SPEED)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_SPATK)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_SPDEF);
    EXPECT_EQ(bst, expected);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="IsBstAvailableAtRank GetSpeciesBST" check`
Expected: FAIL to compile — `battle_hall.h` doesn't exist yet.

- [ ] **Step 3: Write the header**

Create `include/battle_hall.h`:

```c
#ifndef GUARD_BATTLE_HALL_H
#define GUARD_BATTLE_HALL_H

#define HALL_MIN_RANK 1
#define HALL_MAX_RANK 10

bool8 IsBstAvailableAtRank(u32 bst, u8 rank);
u32 GetSpeciesBST(u16 species);
u16 GetHallOpponentSpecies(enum Type type, u8 rank);
u32 CountHallEligibleSpecies(enum Type type, u8 rank);
u16 GetNthHallEligibleSpecies(enum Type type, u8 rank, u32 n);

#endif // GUARD_BATTLE_HALL_H
```

- [ ] **Step 4: Implement `IsBstAvailableAtRank` and `GetSpeciesBST`**

Create `src/battle_hall.c`:

```c
#include "global.h"
#include "battle_hall.h"
#include "pokemon.h"

struct HallBstBracket
{
    u16 minBst;
    u16 maxBst;
    u8 minRank;
};

// BST brackets and their unlock rank, per the source games (Bulbapedia: "List of Battle Hall Pokémon").
// The top bracket has no documented upper bound, so it is left open (maxBst = 0xFFFF) rather than
// guessing a ceiling — the source material only specifies unlock ranks, not an upper cutoff.
static const struct HallBstBracket sHallBstBrackets[] =
{
    { 0,   339,    HALL_MIN_RANK },
    { 340, 439,    3 },
    { 440, 0xFFFF, 6 },
};

#define HALL_BST_BRACKET_COUNT (sizeof(sHallBstBrackets) / sizeof(sHallBstBrackets[0]))

bool8 IsBstAvailableAtRank(u32 bst, u8 rank)
{
    u32 i;

    for (i = 0; i < HALL_BST_BRACKET_COUNT; i++)
    {
        if (bst >= sHallBstBrackets[i].minBst && bst <= sHallBstBrackets[i].maxBst)
            return rank >= sHallBstBrackets[i].minRank;
    }

    return FALSE;
}

u32 GetSpeciesBST(u16 species)
{
    return GetSpeciesBaseStat(species, STAT_HP)
         + GetSpeciesBaseStat(species, STAT_ATK)
         + GetSpeciesBaseStat(species, STAT_DEF)
         + GetSpeciesBaseStat(species, STAT_SPEED)
         + GetSpeciesBaseStat(species, STAT_SPATK)
         + GetSpeciesBaseStat(species, STAT_SPDEF);
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `make TESTS="IsBstAvailableAtRank GetSpeciesBST" check`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add include/battle_hall.h src/battle_hall.c test/battle_hall.c
git commit -m "Add Battle Hall BST-to-rank bracket gating"
```

---

### Task 2: Opponent generation by type and rank

**Files:**
- Modify: `src/battle_hall.c`
- Modify: `test/battle_hall.c`

- [ ] **Step 1: Write the failing test**

Append to `test/battle_hall.c`:

```c
TEST("GetNthHallEligibleSpecies only returns species of the requested type, not banned")
{
    u32 count = CountHallEligibleSpecies(TYPE_WATER, HALL_MAX_RANK);
    u32 i;

    EXPECT_TRUE(count > 0);

    for (i = 0; i < count && i < 20; i++)
    {
        u16 species = GetNthHallEligibleSpecies(TYPE_WATER, HALL_MAX_RANK, i);
        bool8 isWaterType = (gSpeciesInfo[species].types[0] == TYPE_WATER
                           || gSpeciesInfo[species].types[1] == TYPE_WATER);
        EXPECT_TRUE(isWaterType);
        EXPECT_FALSE(gSpeciesInfo[species].isFrontierBanned);
    }
}

TEST("CountHallEligibleSpecies shrinks as rank drops (fewer high-BST mons unlocked)")
{
    u32 countAtMaxRank = CountHallEligibleSpecies(TYPE_NORMAL, HALL_MAX_RANK);
    u32 countAtRank1 = CountHallEligibleSpecies(TYPE_NORMAL, HALL_MIN_RANK);

    EXPECT_TRUE(countAtRank1 <= countAtMaxRank);
}

TEST("GetHallOpponentSpecies always returns a species matching the requested type")
{
    u32 i;

    for (i = 0; i < 20; i++)
    {
        u16 species = GetHallOpponentSpecies(TYPE_FIRE, HALL_MAX_RANK);
        bool8 isFireType = (gSpeciesInfo[species].types[0] == TYPE_FIRE
                          || gSpeciesInfo[species].types[1] == TYPE_FIRE);
        EXPECT_TRUE(isFireType);
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="GetNthHallEligibleSpecies CountHallEligibleSpecies GetHallOpponentSpecies" check`
Expected: FAIL — functions not implemented yet (declared in the header from Task 1 but no body).

- [ ] **Step 3: Implement the generator**

Add to `src/battle_hall.c` (needs `#include "random.h"`):

```c
static bool8 SpeciesIsEligible(u16 species, enum Type type, u8 rank)
{
    if (gSpeciesInfo[species].baseHP == 0)
        return FALSE; // not a real/enabled species
    if (gSpeciesInfo[species].isFrontierBanned)
        return FALSE;
    if (gSpeciesInfo[species].types[0] != type && gSpeciesInfo[species].types[1] != type)
        return FALSE;
    if (!IsBstAvailableAtRank(GetSpeciesBST(species), rank))
        return FALSE;

    return TRUE;
}

u32 CountHallEligibleSpecies(enum Type type, u8 rank)
{
    u32 species;
    u32 count = 0;

    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (SpeciesIsEligible(species, type, rank))
            count++;
    }

    return count;
}

u16 GetNthHallEligibleSpecies(enum Type type, u8 rank, u32 n)
{
    u32 species;
    u32 seen = 0;

    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (SpeciesIsEligible(species, type, rank))
        {
            if (seen == n)
                return species;
            seen++;
        }
    }

    return SPECIES_NONE;
}

u16 GetHallOpponentSpecies(enum Type type, u8 rank)
{
    u32 count = CountHallEligibleSpecies(type, rank);

    if (count == 0)
        return SPECIES_NONE;

    return GetNthHallEligibleSpecies(type, rank, Random() % count);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `make TESTS="GetNthHallEligibleSpecies CountHallEligibleSpecies GetHallOpponentSpecies" check`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/battle_hall.c test/battle_hall.c
git commit -m "Add Battle Hall opponent generation by type and rank"
```

---

### Task 3: Per-type rank persistence

**Files:**
- Modify: `include/global.h`
- Modify: `include/battle_hall.h`
- Modify: `src/battle_hall.c`
- Modify: `test/battle_hall.c`

- [ ] **Step 1: Append the persisted field to `struct BattleFrontier`**

In `include/global.h`, add at the tail (after whatever the Arcade/Castle plans already appended, or after `domePlayerPartyData` if this is the first of the three plans executed):

```c
    /*New*/ u8 hallTypeRanks[NUMBER_OF_MON_TYPES];
```

- [ ] **Step 2: Write the failing test**

Append to `test/battle_hall.c`:

```c
TEST("Hall_GetTypeRank defaults to Rank 1 and Hall_RecordBattleResult advances it on a win")
{
    gSaveBlock2Ptr->frontier.hallTypeRanks[TYPE_GRASS] = 0; // simulate an unset save field
    EXPECT_EQ(Hall_GetTypeRank(TYPE_GRASS), HALL_MIN_RANK);

    Hall_RecordBattleResult(TYPE_GRASS, TRUE);
    EXPECT_EQ(Hall_GetTypeRank(TYPE_GRASS), HALL_MIN_RANK + 1);

    Hall_RecordBattleResult(TYPE_GRASS, FALSE);
    EXPECT_EQ(Hall_GetTypeRank(TYPE_GRASS), HALL_MIN_RANK + 1); // a loss doesn't roll rank back
}

TEST("Hall_GetTypeRank never exceeds HALL_MAX_RANK")
{
    u32 i;

    gSaveBlock2Ptr->frontier.hallTypeRanks[TYPE_WATER] = HALL_MAX_RANK;
    for (i = 0; i < 5; i++)
        Hall_RecordBattleResult(TYPE_WATER, TRUE);

    EXPECT_EQ(Hall_GetTypeRank(TYPE_WATER), HALL_MAX_RANK);
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `make TESTS="Hall_GetTypeRank" check`
Expected: FAIL to compile — functions not declared yet.

- [ ] **Step 4: Add the declarations and implementation**

In `include/battle_hall.h`, add:

```c
u8 Hall_GetTypeRank(enum Type type);
void Hall_RecordBattleResult(enum Type type, bool8 won);
```

In `src/battle_hall.c`, add (needs `#include "event_data.h"` — for `gSaveBlock2Ptr` visibility, same as other frontier files):

```c
u8 Hall_GetTypeRank(enum Type type)
{
    u8 rank = gSaveBlock2Ptr->frontier.hallTypeRanks[type];
    return rank == 0 ? HALL_MIN_RANK : rank;
}

void Hall_RecordBattleResult(enum Type type, bool8 won)
{
    u8 rank = Hall_GetTypeRank(type);

    if (won && rank < HALL_MAX_RANK)
        rank++;

    gSaveBlock2Ptr->frontier.hallTypeRanks[type] = rank;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `make TESTS="Hall_GetTypeRank" check`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add include/global.h include/battle_hall.h src/battle_hall.c test/battle_hall.c
git commit -m "Add Battle Hall per-type rank persistence"
```

---

### Task 4: Debug menu integration

**Files:**
- Modify: `src/debug.c`

This reuses the `sDebugMenu_Actions_BattleFrontier` submenu added in the Battle Arcade plan's Task 6. If neither that plan nor the Battle Castle plan has been executed yet, create the submenu here first following the same pattern shown in the Arcade plan's Task 6, Step 1.

- [ ] **Step 1: Add the menu entry**

In `src/debug.c`, add to `sDebugMenu_Actions_BattleFrontier[]`:

```c
    { COMPOUND_STRING("Start Hall Battle (Normal type)"), DebugAction_BattleFrontier_HallBattle },
```

A fixed type keeps this a one-tap debug entry rather than needing a type-picker submenu; picking a different `enum Type` to pass in is a one-line change for manual testing of other types.

- [ ] **Step 2: Implement the debug action**

The player's Pokémon's own level determines the opponent's level in this phase, matching the design spec's documented simplification of the source game's rank/type-progress-based level formula:

```c
#include "battle_hall.h"

static void DebugAction_BattleFrontier_HallBattle(u8 taskId)
{
    u8 rank;
    u16 species;
    u8 level;

    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateNPCTrainerPartyFromTrainer(gPlayerParty, &sDebugTrainers[DIFFICULTY_NORMAL][DEBUG_TRAINER_PLAYER], TRUE, BATTLE_TYPE_TRAINER);

    rank = Hall_GetTypeRank(TYPE_NORMAL);
    species = GetHallOpponentSpecies(TYPE_NORMAL, rank);
    level = GetMonData(&gPlayerParty[0], MON_DATA_LEVEL, NULL);
    CreateMon(&gEnemyParty[0], species, level, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);

    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gDebugAIFlags = sDebugTrainers[DIFFICULTY_NORMAL][DEBUG_TRAINER_AI].aiFlags;
    gIsDebugBattle = TRUE;
    gBattleEnvironment = BattleSetup_GetEnvironmentId();
    CalculateEnemyPartyCount();
    BattleSetup_StartTrainerBattle_Debug();
    Debug_DestroyMenu_Full(taskId);
}
```

Recording the result (`Hall_RecordBattleResult`) requires the same `CB2_EndTrainerBattle` hook pattern used in the Battle Castle plan's Task 5, Step 3 — add an analogous `gIsDebugHallBattle` flag and battle-type there if you want rank progression to persist across debug battles; for a first pass, calling `Hall_RecordBattleResult` is optional and can be verified by calling it manually from the debug menu after checking the battle outcome.

- [ ] **Step 3: Build the ROM**

Run: `make -j$(nproc)`
Expected: builds cleanly.

- [ ] **Step 4: Manual verification**

Run the ROM in mGBA, open the debug menu, choose **Battle Frontier… → Start Hall Battle (Normal type)**. Confirm the opponent is always a Normal-type species and never a Frontier-banned one (spot-check a few runs).

- [ ] **Step 5: Commit**

```bash
git add src/debug.c
git commit -m "Wire Battle Hall into the debug menu"
```

---

## Plan self-review notes

- Opponent level scaling is deliberately simplified to "matches the player's Pokémon's level" rather than the source games' full rank/type-progress formula — called out explicitly in Task 4 rather than left vague, since the source formula's exact terms weren't fully documented in available references and this keeps the facility functional and testable now.
- Double Battles (same-species requirement) from the spec are not implemented in this plan — the spec's Battle Hall section describes Single Battles as the primary mode and this plan covers that; adding the same-species Double mode is a small follow-up (validate `gPlayerParty[0].species == gPlayerParty[1].species` before allowing entry) once the Single flow is proven out.
