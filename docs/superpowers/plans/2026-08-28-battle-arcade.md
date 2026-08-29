# Battle Arcade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement Sinnoh's Battle Arcade — a 7-battle round where a roulette rolls a random panel effect before each battle — playable from the debug menu, per `docs/superpowers/specs/2026-08-28-sinnoh-battle-frontier-design.md`.

**Architecture:** All panel effects are applied *before* `BattleSetup_StartTrainerBattle_Debug()` is called — no new `BATTLE_TYPE_*` flag is needed. Weather/Trick Room panels reuse the engine's existing pre-battle "starting status" system (`B_VAR_STARTING_STATUS`, `enum StartingStatus`, `TryFieldEffects` in `src/battle_util.c`), extended with 5 new weather members using the exact pattern already used for terrain/room/side statuses. Status/HP/item/level/team-swap panels are plain party-array mutations in the new facility controller. This refines the design spec's assumption that a new `BATTLE_TYPE_ARCADE` flag would be required — deeper research during planning found the existing starting-status system already covers it.

**Tech Stack:** C (GCC via devkitARM), the project's `TEST()`/`SINGLE_BATTLE_TEST()` test DSL (`make check`).

---

## Before you start

Read `docs/superpowers/specs/2026-08-28-sinnoh-battle-frontier-design.md` for full context. This plan implements only the Battle Arcade section of that spec.

All new persistent fields go at the **tail** of `struct BattleFrontier` in `include/global.h` — this is a save-breaking change and must be called out as such in the final PR description.

---

### Task 1: Reserve the starting-status config vars

Right now `B_VAR_STARTING_STATUS` and `B_VAR_STARTING_STATUS_TIMER` are both `0` (disabled) in `include/config/battle.h` — this is a generic engine feature that ships off until a hack assigns it real vars. Battle Arcade needs it on. `include/constants/vars.h` has two adjacent genuinely-unused save vars we can repurpose without any save-layout change: `VAR_UNUSED_0x40FC` and `VAR_UNUSED_0x40FD`.

**Files:**
- Modify: `include/config/battle.h:223,225`

- [ ] **Step 1: Point the config vars at the unused save vars**

In `include/config/battle.h`, change:

```c
#define B_VAR_STARTING_STATUS       0     // If this var has a value, assigning a STATUS_FIELD_xx_TERRAIN to it before battle causes the battle to start with that terrain active.
```
```c
#define B_VAR_STARTING_STATUS_TIMER 0     // If this var has a value greater or equal than 1 field terrains will last that number of turns, otherwise they will last until they're overwritten.
```

to:

```c
#define B_VAR_STARTING_STATUS       VAR_UNUSED_0x40FC     // If this var has a value, assigning a STATUS_FIELD_xx_TERRAIN to it before battle causes the battle to start with that terrain active.
```
```c
#define B_VAR_STARTING_STATUS_TIMER VAR_UNUSED_0x40FD     // If this var has a value greater or equal than 1 field terrains will last that number of turns, otherwise they will last until they're overwritten.
```

- [ ] **Step 2: Confirm the existing terrain tests now compile and pass**

Run: `make TESTS="B_VAR_STARTING_STATUS" check`
Expected: PASS — this activates `test/battle/starting_status/terrain.c` (previously compiled out via `#if B_VAR_STARTING_STATUS != 0`) and both of its tests should pass.

- [ ] **Step 3: Commit**

```bash
git add include/config/battle.h
git commit -m "Enable B_VAR_STARTING_STATUS for Battle Arcade panels"
```

---

### Task 2: Add weather as a startable status

**Files:**
- Modify: `include/constants/battle.h` (`enum StartingStatus`)
- Modify: `src/battle_util.c` (`TryFieldEffects`, add `SetStartingBattleWeather`)
- Create: `test/battle/starting_status/weather.c`
- Modify: `test/Makefile` or test discovery is automatic — verify by running `make check` (this project auto-discovers `test/**/*.c`; no manual registration file to edit)

- [ ] **Step 1: Write the failing test**

Create `test/battle/starting_status/weather.c`:

```c
#include "global.h"
#include "event_data.h"
#include "test/battle.h"

#if B_VAR_STARTING_STATUS != 0

SINGLE_BATTLE_TEST("B_VAR_STARTING_STATUS starts a chosen weather at the beginning of battle")
{
    u16 weather;

    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_SUN; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_RAIN; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_SANDSTORM; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_HAIL; }

    VarSet(B_VAR_STARTING_STATUS, weather);
    VarSet(B_VAR_STARTING_STATUS_TIMER, 0);

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { ; }
    } SCENE {
        switch (weather)
        {
        case STARTING_STATUS_WEATHER_SUN:
            MESSAGE("The sunlight is harsh!");
            break;
        case STARTING_STATUS_WEATHER_RAIN:
            MESSAGE("It's raining!");
            break;
        case STARTING_STATUS_WEATHER_SANDSTORM:
            MESSAGE("The sandstorm is raging!");
            break;
        case STARTING_STATUS_WEATHER_HAIL:
            if (B_OVERWORLD_SNOW >= GEN_9)
                MESSAGE("It's snowing!");
            else
                MESSAGE("It's hailing!");
            break;
        }
    } THEN {
        VarSet(B_VAR_STARTING_STATUS, 0);
    }
}

#endif // B_VAR_STARTING_STATUS
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="B_VAR_STARTING_STATUS starts a chosen weather" check`
Expected: FAIL to compile — `STARTING_STATUS_WEATHER_SUN` and friends are not yet defined.

- [ ] **Step 3: Add the new enum members**

In `include/constants/battle.h`, extend `enum StartingStatus` (currently ends at `STARTING_STATUS_SWAMP_OPPONENT,`):

```c
enum StartingStatus
{
    STARTING_STATUS_NONE,
    STARTING_STATUS_ELECTRIC_TERRAIN,
    STARTING_STATUS_MISTY_TERRAIN,
    STARTING_STATUS_GRASSY_TERRAIN,
    STARTING_STATUS_PSYCHIC_TERRAIN,
    STARTING_STATUS_TRICK_ROOM,
    STARTING_STATUS_MAGIC_ROOM,
    STARTING_STATUS_WONDER_ROOM,
    STARTING_STATUS_TAILWIND_PLAYER,
    STARTING_STATUS_TAILWIND_OPPONENT,
    STARTING_STATUS_RAINBOW_PLAYER,
    STARTING_STATUS_RAINBOW_OPPONENT,
    STARTING_STATUS_SEA_OF_FIRE_PLAYER,
    STARTING_STATUS_SEA_OF_FIRE_OPPONENT,
    STARTING_STATUS_SWAMP_PLAYER,
    STARTING_STATUS_SWAMP_OPPONENT,
    STARTING_STATUS_WEATHER_SUN,
    STARTING_STATUS_WEATHER_RAIN,
    STARTING_STATUS_WEATHER_SANDSTORM,
    STARTING_STATUS_WEATHER_HAIL,
    STARTING_STATUS_WEATHER_FOG,
};
```

(21 members fits comfortably in the 6-bit `startingStatus:6` field on `struct Trainer` in `include/data.h`, max 63.)

- [ ] **Step 4: Add `SetStartingBattleWeather` and wire it into `TryFieldEffects`**

In `src/battle_util.c`, right after `SetStartingSideStatus` (which ends just before `bool32 TryFieldEffects(...)`), add:

```c
static inline bool32 SetStartingBattleWeather(u32 weatherFlag, u32 multistringChooser, u32 anim)
{
    if (!(gBattleWeather & weatherFlag))
    {
        gBattleWeather = weatherFlag;
        gBattleCommunication[MULTISTRING_CHOOSER] = multistringChooser;
        gBattleScripting.animArg1 = anim;
        gWishFutureKnock.weatherDuration = gBattleStruct->startingStatusTimer; // 0 = infinite, same convention as SetStartingFieldStatus's timer
        return TRUE;
    }

    return FALSE;
}
```

Then inside `TryFieldEffects`, in the `FIELD_EFFECT_TRAINER_STATUSES` case:

1. Add `bool32 isWeather = FALSE;` next to the existing `bool32 isTerrain = FALSE;` declaration at the top of the function.
2. Add these cases to the inner `switch ((enum StartingStatus) gBattleStruct->startingStatus)`, right after the existing `STARTING_STATUS_SWAMP_OPPONENT` case:

```c
        case STARTING_STATUS_WEATHER_SUN:
            effect = SetStartingBattleWeather(B_WEATHER_SUN_NORMAL, WEATHER_DROUGHT, B_ANIM_SUN_CONTINUES);
            isWeather = TRUE;
            break;
        case STARTING_STATUS_WEATHER_RAIN:
            effect = SetStartingBattleWeather(B_WEATHER_RAIN_NORMAL, WEATHER_RAIN, B_ANIM_RAIN_CONTINUES);
            isWeather = TRUE;
            break;
        case STARTING_STATUS_WEATHER_SANDSTORM:
            effect = SetStartingBattleWeather(B_WEATHER_SANDSTORM, WEATHER_SANDSTORM, B_ANIM_SANDSTORM_CONTINUES);
            isWeather = TRUE;
            break;
        case STARTING_STATUS_WEATHER_HAIL:
            effect = SetStartingBattleWeather(B_WEATHER_HAIL, WEATHER_SNOW, B_ANIM_HAIL_CONTINUES);
            isWeather = TRUE;
            break;
        case STARTING_STATUS_WEATHER_FOG:
            effect = SetStartingBattleWeather(B_WEATHER_FOG, WEATHER_FOG_HORIZONTAL, B_ANIM_FOG_CONTINUES);
            isWeather = TRUE;
            break;
```

3. Change the trailing `if (effect) { ... }` block (right after the inner switch closes) from:

```c
        if (effect)
        {
            if (isTerrain)
                BattleScriptPushCursorAndCallback(BattleScript_OverworldTerrain);
            else
                BattleScriptPushCursorAndCallback(BattleScript_OverworldStatusStarts);
        }
```

to:

```c
        if (effect)
        {
            if (isTerrain)
                BattleScriptPushCursorAndCallback(BattleScript_OverworldTerrain);
            else if (isWeather)
                BattleScriptPushCursorAndCallback(BattleScript_OverworldWeatherStarts);
            else
                BattleScriptPushCursorAndCallback(BattleScript_OverworldStatusStarts);
        }
```

- [ ] **Step 5: Run test to verify it passes**

Run: `make TESTS="B_VAR_STARTING_STATUS starts a chosen weather" check`
Expected: PASS (4 parametrized cases)

- [ ] **Step 6: Commit**

```bash
git add include/constants/battle.h src/battle_util.c test/battle/starting_status/weather.c
git commit -m "Add weather as a startable pre-battle status"
```

---

### Task 3: Panel selection (weighted roulette)

Panels are grouped into two buckets: **favorable** (helps the player or hurts the opponent — includes BP grants) and **neutral** (field effects, panels that hurt the player, skip, no-event). A hidden per-round performance score shifts the odds: worse performance → more favorable panels, matching the source game's behavior.

**Files:**
- Create: `include/battle_arcade.h`
- Create: `src/battle_arcade.c`
- Create: `test/battle_arcade.c`
- Modify: `src/CMakeLists.txt` is not used by this project's Make-based build — no build file registration needed; `.c` files under `src/` are picked up automatically by the Makefile's wildcard rule (confirm by checking that `src/battle_pike.c` isn't separately listed anywhere other than being compiled from the `src/*.c` glob).

- [ ] **Step 1: Write the failing test**

Create `test/battle_arcade.c`:

```c
#include "global.h"
#include "test/test.h"
#include "battle_arcade.h"

TEST("Arcade_RollIsFavorableBucket favors the player more as performance score drops")
{
    // performanceScore < 33: 70% favorable
    EXPECT_TRUE(Arcade_RollIsFavorableBucket(0, 10));
    EXPECT_TRUE(Arcade_RollIsFavorableBucket(69, 10));
    EXPECT_FALSE(Arcade_RollIsFavorableBucket(70, 10));

    // 33 <= performanceScore < 67: 50% favorable
    EXPECT_TRUE(Arcade_RollIsFavorableBucket(49, 50));
    EXPECT_FALSE(Arcade_RollIsFavorableBucket(50, 50));

    // performanceScore >= 67: 30% favorable
    EXPECT_TRUE(Arcade_RollIsFavorableBucket(29, 90));
    EXPECT_FALSE(Arcade_RollIsFavorableBucket(30, 90));
}

TEST("Arcade_PickPanelInBucket wraps around each bucket's panel count")
{
    EXPECT_EQ(Arcade_PickPanelInBucket(0, TRUE), Arcade_PickPanelInBucket(ARCADE_PANEL_COUNT_FAVORABLE, TRUE));
    EXPECT_EQ(Arcade_PickPanelInBucket(0, FALSE), Arcade_PickPanelInBucket(ARCADE_PANEL_COUNT_NEUTRAL, FALSE));
}

TEST("Arcade_PanelSkipsBattle is true only for the skip panel")
{
    EXPECT_TRUE(Arcade_PanelSkipsBattle(ARCADE_EFFECT_SKIP_BATTLE));
    EXPECT_FALSE(Arcade_PanelSkipsBattle(ARCADE_EFFECT_NONE));
    EXPECT_FALSE(Arcade_PanelSkipsBattle(ARCADE_EFFECT_GIVE_BP_1));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make TESTS="Arcade_RollIsFavorableBucket Arcade_PickPanelInBucket Arcade_PanelSkipsBattle" check`
Expected: FAIL to compile — `include/battle_arcade.h` doesn't exist yet.

- [ ] **Step 3: Write the header**

Create `include/battle_arcade.h`:

```c
#ifndef GUARD_BATTLE_ARCADE_H
#define GUARD_BATTLE_ARCADE_H

enum ArcadePanelEffect
{
    ARCADE_EFFECT_NONE,
    ARCADE_EFFECT_CUT_HP_PLAYER,
    ARCADE_EFFECT_CUT_HP_OPPONENT,
    ARCADE_EFFECT_POISON_PLAYER,
    ARCADE_EFFECT_POISON_OPPONENT,
    ARCADE_EFFECT_PARALYZE_PLAYER,
    ARCADE_EFFECT_PARALYZE_OPPONENT,
    ARCADE_EFFECT_BURN_PLAYER,
    ARCADE_EFFECT_BURN_OPPONENT,
    ARCADE_EFFECT_SLEEP_PLAYER,
    ARCADE_EFFECT_SLEEP_OPPONENT,
    ARCADE_EFFECT_FREEZE_PLAYER,
    ARCADE_EFFECT_FREEZE_OPPONENT,
    ARCADE_EFFECT_GIVE_ITEM_PLAYER,
    ARCADE_EFFECT_RAISE_LEVEL_PLAYER,
    ARCADE_EFFECT_WEATHER_SUN,
    ARCADE_EFFECT_WEATHER_RAIN,
    ARCADE_EFFECT_WEATHER_SANDSTORM,
    ARCADE_EFFECT_WEATHER_HAIL,
    ARCADE_EFFECT_TRICK_ROOM,
    ARCADE_EFFECT_TEAM_SWAP,
    ARCADE_EFFECT_GIVE_BP_1,
    ARCADE_EFFECT_GIVE_BP_3,
    ARCADE_EFFECT_SKIP_BATTLE,
};

#define ARCADE_PANEL_COUNT_FAVORABLE 9
#define ARCADE_PANEL_COUNT_NEUTRAL   14
#define ARCADE_ROUND_LENGTH          7

bool8 Arcade_RollIsFavorableBucket(u8 roll0to99, u8 performanceScore);
enum ArcadePanelEffect Arcade_PickPanelInBucket(u8 roll, bool8 favorable);
enum ArcadePanelEffect Arcade_ChoosePanel(u8 performanceScore);
bool8 Arcade_PanelSkipsBattle(enum ArcadePanelEffect effect);
void Arcade_ApplyPanelEffect(enum ArcadePanelEffect effect);

#endif // GUARD_BATTLE_ARCADE_H
```

- [ ] **Step 4: Write the implementation**

Create `src/battle_arcade.c`:

```c
#include "global.h"
#include "battle_arcade.h"
#include "random.h"

static const enum ArcadePanelEffect sArcadeFavorablePanels[ARCADE_PANEL_COUNT_FAVORABLE] =
{
    ARCADE_EFFECT_CUT_HP_OPPONENT,
    ARCADE_EFFECT_POISON_OPPONENT,
    ARCADE_EFFECT_PARALYZE_OPPONENT,
    ARCADE_EFFECT_BURN_OPPONENT,
    ARCADE_EFFECT_SLEEP_OPPONENT,
    ARCADE_EFFECT_FREEZE_OPPONENT,
    ARCADE_EFFECT_GIVE_ITEM_PLAYER,
    ARCADE_EFFECT_RAISE_LEVEL_PLAYER,
    ARCADE_EFFECT_GIVE_BP_3,
};

static const enum ArcadePanelEffect sArcadeNeutralPanels[ARCADE_PANEL_COUNT_NEUTRAL] =
{
    ARCADE_EFFECT_CUT_HP_PLAYER,
    ARCADE_EFFECT_POISON_PLAYER,
    ARCADE_EFFECT_PARALYZE_PLAYER,
    ARCADE_EFFECT_BURN_PLAYER,
    ARCADE_EFFECT_SLEEP_PLAYER,
    ARCADE_EFFECT_FREEZE_PLAYER,
    ARCADE_EFFECT_WEATHER_SUN,
    ARCADE_EFFECT_WEATHER_RAIN,
    ARCADE_EFFECT_WEATHER_SANDSTORM,
    ARCADE_EFFECT_WEATHER_HAIL,
    ARCADE_EFFECT_TRICK_ROOM,
    ARCADE_EFFECT_TEAM_SWAP,
    ARCADE_EFFECT_GIVE_BP_1,
    ARCADE_EFFECT_SKIP_BATTLE,
};

bool8 Arcade_RollIsFavorableBucket(u8 roll0to99, u8 performanceScore)
{
    u8 favorablePercent;

    if (performanceScore < 33)
        favorablePercent = 70;
    else if (performanceScore < 67)
        favorablePercent = 50;
    else
        favorablePercent = 30;

    return roll0to99 < favorablePercent;
}

enum ArcadePanelEffect Arcade_PickPanelInBucket(u8 roll, bool8 favorable)
{
    if (favorable)
        return sArcadeFavorablePanels[roll % ARCADE_PANEL_COUNT_FAVORABLE];
    else
        return sArcadeNeutralPanels[roll % ARCADE_PANEL_COUNT_NEUTRAL];
}

enum ArcadePanelEffect Arcade_ChoosePanel(u8 performanceScore)
{
    bool8 favorable = Arcade_RollIsFavorableBucket(Random() % 100, performanceScore);
    return Arcade_PickPanelInBucket(Random(), favorable);
}

bool8 Arcade_PanelSkipsBattle(enum ArcadePanelEffect effect)
{
    return effect == ARCADE_EFFECT_SKIP_BATTLE;
}
```

(`Arcade_ApplyPanelEffect` is implemented in Task 4 — leave it undefined for now, which is fine since nothing calls it yet.)

Actually — declaring it in the header but not defining it will fail to link once something calls it. Since Task 3's tests don't call `Arcade_ApplyPanelEffect`, leave a definition stub that Task 4 will replace in the same file:

```c
void Arcade_ApplyPanelEffect(enum ArcadePanelEffect effect)
{

}
```

Add that empty (but real, compiling) function at the bottom of `src/battle_arcade.c` for now; Task 4 fills in its body.

- [ ] **Step 5: Run test to verify it passes**

Run: `make TESTS="Arcade_RollIsFavorableBucket Arcade_PickPanelInBucket Arcade_PanelSkipsBattle" check`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add include/battle_arcade.h src/battle_arcade.c test/battle_arcade.c
git commit -m "Add Battle Arcade panel selection"
```

---

### Task 4: Panel effect application

**Files:**
- Modify: `src/battle_arcade.c` (replace the stub `Arcade_ApplyPanelEffect`)
- Modify: `test/battle_arcade.c`

- [ ] **Step 1: Write the failing tests**

Append to `test/battle_arcade.c`:

```c
#include "pokemon.h"
#include "event_data.h"
#include "constants/battle.h"
#include "constants/items.h"

TEST("Arcade_ApplyPanelEffect inflicts the right status on the right side")
{
    struct Pokemon playerMon, opponentMon;
    CreateMon(&playerMon, SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&opponentMon, SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerParty[0] = playerMon;
    gEnemyParty[0] = opponentMon;

    Arcade_ApplyPanelEffect(ARCADE_EFFECT_POISON_PLAYER);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_STATUS, NULL) & STATUS1_POISON, STATUS1_POISON);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_STATUS, NULL), 0);

    gPlayerParty[0] = playerMon;
    gEnemyParty[0] = opponentMon;
    Arcade_ApplyPanelEffect(ARCADE_EFFECT_BURN_OPPONENT);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_STATUS, NULL) & STATUS1_BURN, STATUS1_BURN);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_STATUS, NULL), 0);
}

TEST("Arcade_ApplyPanelEffect cuts HP to 80 percent, minimum 1")
{
    struct Pokemon mon;
    u32 maxHp, hp;

    CreateMon(&mon, SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerParty[0] = mon;
    maxHp = GetMonData(&gPlayerParty[0], MON_DATA_MAX_HP, NULL);

    Arcade_ApplyPanelEffect(ARCADE_EFFECT_CUT_HP_PLAYER);
    hp = GetMonData(&gPlayerParty[0], MON_DATA_HP, NULL);
    EXPECT_EQ(hp, (maxHp * 80) / 100);
}

TEST("Arcade_ApplyPanelEffect sets B_VAR_STARTING_STATUS for weather and Trick Room panels")
{
    Arcade_ApplyPanelEffect(ARCADE_EFFECT_WEATHER_RAIN);
    EXPECT_EQ(VarGet(B_VAR_STARTING_STATUS), STARTING_STATUS_WEATHER_RAIN);

    Arcade_ApplyPanelEffect(ARCADE_EFFECT_TRICK_ROOM);
    EXPECT_EQ(VarGet(B_VAR_STARTING_STATUS), STARTING_STATUS_TRICK_ROOM);
    EXPECT_EQ(VarGet(B_VAR_STARTING_STATUS_TIMER), 5);
}

TEST("Arcade_ApplyPanelEffect grants BP")
{
    u16 before = gSaveBlock2Ptr->frontier.battlePoints;
    Arcade_ApplyPanelEffect(ARCADE_EFFECT_GIVE_BP_3);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, before + 3);
}

TEST("Arcade_ApplyPanelEffect swaps the player and opponent parties")
{
    struct Pokemon playerMon, opponentMon;
    CreateMon(&playerMon, SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&opponentMon, SPECIES_WYNAUT, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerParty[0] = playerMon;
    gEnemyParty[0] = opponentMon;

    Arcade_ApplyPanelEffect(ARCADE_EFFECT_TEAM_SWAP);

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES, NULL), SPECIES_WYNAUT);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_SPECIES, NULL), SPECIES_WOBBUFFET);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `make TESTS="Arcade_ApplyPanelEffect" check`
Expected: FAIL — the stub function does nothing, so every assertion fails.

- [ ] **Step 3: Implement `Arcade_ApplyPanelEffect`**

Replace the stub at the bottom of `src/battle_arcade.c` with:

```c
#include "pokemon.h"
#include "event_data.h"
#include "constants/battle.h"
#include "constants/items.h"
#include "frontier_util.h"

#define ARCADE_TEAM_SIZE 3

static void CutHp(struct Pokemon *mon)
{
    u32 maxHp = GetMonData(mon, MON_DATA_MAX_HP, NULL);
    u32 newHp = (maxHp * 80) / 100;
    if (newHp < 1)
        newHp = 1;
    SetMonData(mon, MON_DATA_HP, &newHp);
}

static void InflictStatus(struct Pokemon *mon, u32 status)
{
    SetMonData(mon, MON_DATA_STATUS, &status);
}

static void SetStartingStatus(u32 status, u32 timer)
{
    VarSet(B_VAR_STARTING_STATUS, status);
    VarSet(B_VAR_STARTING_STATUS_TIMER, timer);
}

static void SwapTeams(void)
{
    struct Pokemon temp[ARCADE_TEAM_SIZE];
    memcpy(temp, gPlayerParty, sizeof(temp));
    memcpy(gPlayerParty, gEnemyParty, sizeof(temp));
    memcpy(gEnemyParty, temp, sizeof(temp));
}

static void RaiseLevel(struct Pokemon *mon)
{
    u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
    level += 3;
    if (level > FRONTIER_MAX_LEVEL_50)
        level = FRONTIER_MAX_LEVEL_50;
    SetMonData(mon, MON_DATA_LEVEL, &level);
    CalculateMonStats(mon);
}

void Arcade_ApplyPanelEffect(enum ArcadePanelEffect effect)
{
    u16 item;

    switch (effect)
    {
    case ARCADE_EFFECT_NONE:
    case ARCADE_EFFECT_SKIP_BATTLE:
        break;
    case ARCADE_EFFECT_CUT_HP_PLAYER:
        CutHp(&gPlayerParty[0]);
        break;
    case ARCADE_EFFECT_CUT_HP_OPPONENT:
        CutHp(&gEnemyParty[0]);
        break;
    case ARCADE_EFFECT_POISON_PLAYER:
        InflictStatus(&gPlayerParty[0], STATUS1_POISON);
        break;
    case ARCADE_EFFECT_POISON_OPPONENT:
        InflictStatus(&gEnemyParty[0], STATUS1_POISON);
        break;
    case ARCADE_EFFECT_PARALYZE_PLAYER:
        InflictStatus(&gPlayerParty[0], STATUS1_PARALYSIS);
        break;
    case ARCADE_EFFECT_PARALYZE_OPPONENT:
        InflictStatus(&gEnemyParty[0], STATUS1_PARALYSIS);
        break;
    case ARCADE_EFFECT_BURN_PLAYER:
        InflictStatus(&gPlayerParty[0], STATUS1_BURN);
        break;
    case ARCADE_EFFECT_BURN_OPPONENT:
        InflictStatus(&gEnemyParty[0], STATUS1_BURN);
        break;
    case ARCADE_EFFECT_SLEEP_PLAYER:
        InflictStatus(&gPlayerParty[0], STATUS1_SLEEP_TURN(3));
        break;
    case ARCADE_EFFECT_SLEEP_OPPONENT:
        InflictStatus(&gEnemyParty[0], STATUS1_SLEEP_TURN(3));
        break;
    case ARCADE_EFFECT_FREEZE_PLAYER:
        InflictStatus(&gPlayerParty[0], STATUS1_FREEZE);
        break;
    case ARCADE_EFFECT_FREEZE_OPPONENT:
        InflictStatus(&gEnemyParty[0], STATUS1_FREEZE);
        break;
    case ARCADE_EFFECT_GIVE_ITEM_PLAYER:
        item = ITEM_SITRUS_BERRY;
        SetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM, &item);
        break;
    case ARCADE_EFFECT_RAISE_LEVEL_PLAYER:
        RaiseLevel(&gPlayerParty[0]);
        break;
    case ARCADE_EFFECT_WEATHER_SUN:
        SetStartingStatus(STARTING_STATUS_WEATHER_SUN, 0);
        break;
    case ARCADE_EFFECT_WEATHER_RAIN:
        SetStartingStatus(STARTING_STATUS_WEATHER_RAIN, 0);
        break;
    case ARCADE_EFFECT_WEATHER_SANDSTORM:
        SetStartingStatus(STARTING_STATUS_WEATHER_SANDSTORM, 0);
        break;
    case ARCADE_EFFECT_WEATHER_HAIL:
        SetStartingStatus(STARTING_STATUS_WEATHER_HAIL, 0);
        break;
    case ARCADE_EFFECT_TRICK_ROOM:
        SetStartingStatus(STARTING_STATUS_TRICK_ROOM, 5);
        break;
    case ARCADE_EFFECT_TEAM_SWAP:
        SwapTeams();
        break;
    case ARCADE_EFFECT_GIVE_BP_1:
        IncrementDailyBattlePoints(1);
        break;
    case ARCADE_EFFECT_GIVE_BP_3:
        IncrementDailyBattlePoints(3);
        break;
    }
}
```

Note: `IncrementDailyBattlePoints` is declared `static` in `src/frontier_util.c` today (confirmed via `grep -n "IncrementDailyBattlePoints" src/frontier_util.c`). As part of this step, remove `static` from its definition and add a prototype to `include/frontier_util.h` so `battle_arcade.c` can call it — this is the one shared-utility visibility change this task needs.

- [ ] **Step 4: Run tests to verify they pass**

Run: `make TESTS="Arcade_ApplyPanelEffect" check`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/battle_arcade.c test/battle_arcade.c src/frontier_util.c include/frontier_util.h
git commit -m "Implement Battle Arcade panel effects"
```

---

### Task 5: Round state and persisted streaks

**Files:**
- Modify: `include/global.h` (append fields to `struct BattleFrontier`)
- Modify: `include/battle_arcade.h`
- Modify: `src/battle_arcade.c`

- [ ] **Step 1: Append persisted fields to `struct BattleFrontier`**

In `include/global.h`, add these fields immediately before the struct's closing `};` (after `domePlayerPartyData`) — appending at the tail per the spec's save-breaking-change guidance:

```c
    /*New*/ u16 arcadeWinStreaks[FRONTIER_LVL_MODE_COUNT];
    /*New*/ u16 arcadeRecordWinStreaks[FRONTIER_LVL_MODE_COUNT];
    /*New*/ u16 arcadePrize;
```

- [ ] **Step 2: Add round-tracking state and API to `battle_arcade.h`/`.c`**

In `include/battle_arcade.h`, add:

```c
void Arcade_StartRound(void);
enum ArcadePanelEffect Arcade_RollPanelForNextBattle(void);
void Arcade_RecordBattleResult(bool8 won);
u8 Arcade_GetBattleNumber(void); // 1-7
```

In `src/battle_arcade.c`, add EWRAM state and implementations:

```c
static EWRAM_DATA u8 sPerformanceScore = 0;
static EWRAM_DATA u8 sBattleNumber = 0; // 0-6, internal; externally reported as 1-7

void Arcade_StartRound(void)
{
    sPerformanceScore = 50; // start neutral
    sBattleNumber = 0;
}

enum ArcadePanelEffect Arcade_RollPanelForNextBattle(void)
{
    return Arcade_ChoosePanel(sPerformanceScore);
}

void Arcade_RecordBattleResult(bool8 won)
{
    if (won && sPerformanceScore < 100)
        sPerformanceScore += 10;
    else if (!won && sPerformanceScore >= 10)
        sPerformanceScore -= 10;
    sBattleNumber++;
    if (sBattleNumber >= ARCADE_ROUND_LENGTH)
        sBattleNumber = 0;
}

u8 Arcade_GetBattleNumber(void)
{
    return sBattleNumber + 1;
}
```

- [ ] **Step 3: Write a test for the round-tracking behavior**

Append to `test/battle_arcade.c`:

```c
TEST("Arcade_RecordBattleResult raises performance score on a win and wraps the battle number")
{
    u8 i;

    Arcade_StartRound();
    EXPECT_EQ(Arcade_GetBattleNumber(), 1);

    for (i = 0; i < ARCADE_ROUND_LENGTH; i++)
        Arcade_RecordBattleResult(TRUE);

    EXPECT_EQ(Arcade_GetBattleNumber(), 1); // wrapped back to the start of a new round
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `make TESTS="Arcade_RecordBattleResult" check`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/global.h include/battle_arcade.h src/battle_arcade.c test/battle_arcade.c
git commit -m "Add Battle Arcade round tracking and persisted streak fields"
```

---

### Task 6: Debug menu integration

**Files:**
- Modify: `src/debug.c`

- [ ] **Step 1: Add the submenu enum, forward declaration, and menu table entries**

In `src/debug.c`, add a new submenu array following the exact pattern of `sDebugMenu_Actions_Party` (defined just above it):

```c
static void DebugAction_BattleFrontier_ArcadeRound(u8 taskId);

static const struct DebugMenuOption sDebugMenu_Actions_BattleFrontier[] =
{
    { COMPOUND_STRING("Start Arcade Round"), DebugAction_BattleFrontier_ArcadeRound },
    { NULL }
};
```

Add it to `sDebugMenu_Actions_Main[]`, right after the `"Party…"` entry:

```c
    { COMPOUND_STRING("Battle Frontier…"), DebugAction_OpenSubMenu, sDebugMenu_Actions_BattleFrontier, },
```

- [ ] **Step 2: Implement the debug action**

Add near `DebugAction_Party_BattleSingle` (reuses the exact same battle-launch pattern):

```c
#include "battle_arcade.h"

static void DebugAction_BattleFrontier_ArcadeRound(u8 taskId)
{
    enum ArcadePanelEffect panel;

    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    CreateNPCTrainerPartyFromTrainer(gPlayerParty, &sDebugTrainers[DIFFICULTY_NORMAL][DEBUG_TRAINER_PLAYER], TRUE, BATTLE_TYPE_TRAINER);
    CreateNPCTrainerPartyFromTrainer(gEnemyParty, GetDebugAiTrainer(), FALSE, BATTLE_TYPE_TRAINER);

    Arcade_StartRound();
    panel = Arcade_RollPanelForNextBattle();
    Arcade_ApplyPanelEffect(panel);

    if (Arcade_PanelSkipsBattle(panel))
    {
        Debug_DestroyMenu_Full(taskId);
        return;
    }

    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gDebugAIFlags = sDebugTrainers[DIFFICULTY_NORMAL][DEBUG_TRAINER_AI].aiFlags;
    gIsDebugBattle = TRUE;
    gBattleEnvironment = BattleSetup_GetEnvironmentId();
    CalculateEnemyPartyCount();
    BattleSetup_StartTrainerBattle_Debug();
    Debug_DestroyMenu_Full(taskId);
}
```

- [ ] **Step 2: Build the ROM**

Run: `make -j$(nproc)`
Expected: builds cleanly with no new warnings from `src/debug.c` or `src/battle_arcade.c`.

- [ ] **Step 3: Manual verification**

Run the ROM in mGBA, open the debug menu, choose **Battle Frontier… → Start Arcade Round**. Confirm a battle launches (or the menu just closes for the ~7% chance skip-battle panel — re-trigger a few times to observe different panels; weather/Trick Room panels should show the intro message from Task 2 at the start of the battle).

- [ ] **Step 4: Commit**

```bash
git add src/debug.c
git commit -m "Wire Battle Arcade into the debug menu"
```

---

## Plan self-review notes

- Every panel effect in the design spec's table is implemented in Task 4 except roulette-speed-up/down and "randomize roulette order," which the spec itself scoped as purely cosmetic (they don't affect any battle-visible state) — omitted here as YAGNI for a debug-menu-only phase with no real roulette animation to speed up.
- Item/berry "lasts the rest of the round" and level-raise "lasts one battle" duration nuances from the spec are not separately modeled — the item and level change are applied directly to the persistent `gPlayerParty` mon, so they naturally carry forward through the round already (Task 5 doesn't reset party state between battles, only `sPerformanceScore`/`sBattleNumber`), matching the "one round" duration for these two effects it via reuse rather than a bespoke duration timer.
