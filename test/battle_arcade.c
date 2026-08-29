#include "global.h"
#include "test/test.h"
#include "battle_arcade.h"

TEST("Arcade_RollIsFavorableBucket favors the player more as performance score drops")
{
    // performanceScore < 33: 70% favorable
    EXPECT_EQ(Arcade_RollIsFavorableBucket(0, 10), TRUE);
    EXPECT_EQ(Arcade_RollIsFavorableBucket(69, 10), TRUE);
    EXPECT_EQ(Arcade_RollIsFavorableBucket(70, 10), FALSE);

    // 33 <= performanceScore < 67: 50% favorable
    EXPECT_EQ(Arcade_RollIsFavorableBucket(49, 50), TRUE);
    EXPECT_EQ(Arcade_RollIsFavorableBucket(50, 50), FALSE);

    // performanceScore >= 67: 30% favorable
    EXPECT_EQ(Arcade_RollIsFavorableBucket(29, 90), TRUE);
    EXPECT_EQ(Arcade_RollIsFavorableBucket(30, 90), FALSE);
}

TEST("Arcade_PickPanelInBucket wraps around each bucket's panel count")
{
    EXPECT_EQ(Arcade_PickPanelInBucket(0, TRUE), Arcade_PickPanelInBucket(ARCADE_PANEL_COUNT_FAVORABLE, TRUE));
    EXPECT_EQ(Arcade_PickPanelInBucket(0, FALSE), Arcade_PickPanelInBucket(ARCADE_PANEL_COUNT_NEUTRAL, FALSE));
}

TEST("Arcade_PanelSkipsBattle is true only for the skip panel")
{
    EXPECT_EQ(Arcade_PanelSkipsBattle(ARCADE_EFFECT_SKIP_BATTLE), TRUE);
    EXPECT_EQ(Arcade_PanelSkipsBattle(ARCADE_EFFECT_NONE), FALSE);
    EXPECT_EQ(Arcade_PanelSkipsBattle(ARCADE_EFFECT_GIVE_BP_1), FALSE);
}

#include "pokemon.h"
#include "event_data.h"
#include "constants/battle.h"
#include "constants/items.h"
#include "constants/battle_frontier.h"

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

TEST("Arcade_ApplyPanelEffect clamps BP to MAX_BATTLE_FRONTIER_POINTS instead of overflowing")
{
    gSaveBlock2Ptr->frontier.battlePoints = MAX_BATTLE_FRONTIER_POINTS - 1;
    Arcade_ApplyPanelEffect(ARCADE_EFFECT_GIVE_BP_3);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.battlePoints, MAX_BATTLE_FRONTIER_POINTS);
}

TEST("Arcade_ApplyPanelEffect raises level by 3, capped at FRONTIER_MAX_LEVEL_50")
{
    struct Pokemon mon;
    u8 level = FRONTIER_MAX_LEVEL_50 - 1;

    CreateMon(&mon, SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerParty[0] = mon;
    SetMonData(&gPlayerParty[0], MON_DATA_LEVEL, &level);

    Arcade_ApplyPanelEffect(ARCADE_EFFECT_RAISE_LEVEL_PLAYER);

    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_LEVEL, NULL), FRONTIER_MAX_LEVEL_50);
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
