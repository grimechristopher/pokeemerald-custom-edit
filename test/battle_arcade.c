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
