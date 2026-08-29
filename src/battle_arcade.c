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

void Arcade_ApplyPanelEffect(enum ArcadePanelEffect effect)
{
    // TODO(Task 4): apply the panel's effect to the battle state. Intentionally empty stub.
}
