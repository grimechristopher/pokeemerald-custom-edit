#include "global.h"
#include "battle_arcade.h"
#include "random.h"
#include "pokemon.h"
#include "event_data.h"
#include "constants/battle.h"
#include "constants/items.h"
#include "constants/battle_frontier.h"
#include "tv.h"

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
    struct Pokemon temp[FRONTIER_PARTY_SIZE];
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

// Mirrors the increment+clamp+daily-tracking pattern used by GiveBattlePoints (frontier_util.c),
// and the same MAX_BATTLE_FRONTIER_POINTS clamp used by GiveFrontierBattlePoints (field_specials.c).
static void GiveArcadeBattlePoints(u16 amount)
{
    u32 points = gSaveBlock2Ptr->frontier.battlePoints + amount;
    if (points > MAX_BATTLE_FRONTIER_POINTS)
        points = MAX_BATTLE_FRONTIER_POINTS;
    gSaveBlock2Ptr->frontier.battlePoints = points;
    IncrementDailyBattlePoints(amount);
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
        GiveArcadeBattlePoints(1);
        break;
    case ARCADE_EFFECT_GIVE_BP_3:
        GiveArcadeBattlePoints(3);
        break;
    }
}

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

u8 Arcade_GetPerformanceScore(void)
{
    return sPerformanceScore;
}
