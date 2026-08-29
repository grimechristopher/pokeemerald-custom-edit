#include "global.h"
#include "test/test.h"
#include "battle_castle.h"

TEST("CalculateCastlePoints awards full per-mon bonuses for a clean sweep")
{
    struct CastleMonResult mons[FRONTIER_PARTY_SIZE] =
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
    struct CastleMonResult mons[FRONTIER_PARTY_SIZE] =
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
    struct CastleMonResult mons[FRONTIER_PARTY_SIZE] =
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
