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

TEST("CastleShop_TrySpend deducts CP only when affordable")
{
    u32 cp = 20;

    EXPECT(CastleShop_TrySpend(&cp, CASTLE_COST_SCOUT_SPECIES));
    EXPECT_EQ(cp, 19);

    EXPECT(CastleShop_TrySpend(&cp, CASTLE_COST_HEAL_FULL));
    EXPECT_EQ(cp, 19 - CASTLE_COST_HEAL_FULL);
}

TEST("CastleShop_TrySpend refuses to go negative")
{
    u32 cp = 2;

    EXPECT(!CastleShop_TrySpend(&cp, CASTLE_COST_HEAL_FULL));
    EXPECT_EQ(cp, 2); // unchanged
}

#include "pokemon.h"
#include "constants/battle.h"

TEST("GetCastleMonResults reads fainted state, HP percent, and status from a party")
{
    struct CastleMonResult results[FRONTIER_PARTY_SIZE];
    struct Pokemon party[FRONTIER_PARTY_SIZE];
    u32 i;
    u32 hp;
    u32 status;

    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        CreateMon(&party[i], SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);

    hp = 0;
    SetMonData(&party[0], MON_DATA_HP, &hp); // faint mon 0

    status = STATUS1_POISON;
    SetMonData(&party[2], MON_DATA_STATUS, &status); // poison mon 2

    GetCastleMonResults(party, results);

    EXPECT(results[0].fainted);
    EXPECT_EQ(results[0].hpPercent, 0);
    EXPECT(!results[0].hasStatus);
    EXPECT(!results[1].fainted);
    EXPECT_EQ(results[1].hpPercent, 100);
    EXPECT(!results[1].hasStatus);
    EXPECT(!results[2].fainted);
    EXPECT(results[2].hasStatus);
}

#include "event_data.h"
#include "constants/battle_frontier.h"

TEST("Castle_StartChallenge resets session CP to CASTLE_STARTING_CP")
{
    Castle_StartChallenge();
    EXPECT_EQ(Castle_GetCurrentCP(), CASTLE_STARTING_CP);
}

TEST("Castle_TrySpendCP spends from the session CP set up by Castle_StartChallenge")
{
    Castle_StartChallenge();

    EXPECT(Castle_TrySpendCP(CASTLE_COST_SCOUT_SPECIES));
    EXPECT_EQ(Castle_GetCurrentCP(), CASTLE_STARTING_CP - CASTLE_COST_SCOUT_SPECIES);
}

TEST("Castle_ApplyBattleResult resets CP and streak to starting values on a loss")
{
    struct Pokemon party[FRONTIER_PARTY_SIZE];
    u32 i;

    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        CreateMon(&party[i], SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);

    Castle_StartChallenge();
    Castle_TrySpendCP(CASTLE_COST_SCOUT_SPECIES); // spend some CP so the reset is observable

    Castle_ApplyBattleResult(FALSE, party, 0, 0);

    EXPECT_EQ(Castle_GetCurrentCP(), CASTLE_STARTING_CP);
}

TEST("Castle_ApplyBattleResult adds CalculateCastlePoints' result to CP and raises the win streak/record on a win")
{
    struct Pokemon party[FRONTIER_PARTY_SIZE];
    u32 i;
    u16 streakBefore;

    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        CreateMon(&party[i], SPECIES_WOBBUFFET, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);

    gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_50] = 0;
    gSaveBlock2Ptr->frontier.castleRecordWinStreaks[FRONTIER_LVL_50] = 0;
    streakBefore = gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_50];

    Castle_StartChallenge();
    // Full-HP, unfainted, status-free party with 0 PP used, 0 levels raised: CalculateCastlePoints == 29 (see above).
    Castle_ApplyBattleResult(TRUE, party, 0, 0);

    EXPECT_EQ(Castle_GetCurrentCP(), CASTLE_STARTING_CP + 29);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_50], streakBefore + 1);
    EXPECT_EQ(gSaveBlock2Ptr->frontier.castleRecordWinStreaks[FRONTIER_LVL_50], streakBefore + 1);
    EXPECT_EQ(Castle_GetWinStreak(FRONTIER_LVL_50), streakBefore + 1);
}
