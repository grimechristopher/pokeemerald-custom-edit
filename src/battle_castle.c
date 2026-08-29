#include "global.h"
#include "battle_castle.h"
#include "pokemon.h"
#include "constants/battle_frontier.h"

static EWRAM_DATA u32 sCurrentCP = 0;
static EWRAM_DATA u16 sCurrentStreak = 0;

EWRAM_DATA bool8 gIsDebugCastleBattle = FALSE;

u32 CalculateCastlePoints(const struct CastleMonResult mons[FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised)
{
    u32 i;
    u32 points = 0;

    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
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

void GetCastleMonResults(struct Pokemon party[FRONTIER_PARTY_SIZE], struct CastleMonResult results[FRONTIER_PARTY_SIZE])
{
    u32 i;

    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        u32 hp = GetMonData(&party[i], MON_DATA_HP, NULL);
        u32 maxHp = GetMonData(&party[i], MON_DATA_MAX_HP, NULL);

        results[i].fainted = (hp == 0);
        results[i].hpPercent = maxHp == 0 ? 0 : (hp * 100) / maxHp;
        results[i].hasStatus = (GetMonData(&party[i], MON_DATA_STATUS, NULL) != 0);
    }
}

bool8 CastleShop_TrySpend(u32 *cp, u32 cost)
{
    if (*cp < cost)
        return FALSE;

    *cp -= cost;
    return TRUE;
}

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

void Castle_ApplyBattleResult(bool8 won, struct Pokemon party[FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised)
{
    struct CastleMonResult results[FRONTIER_PARTY_SIZE];

    if (!won)
    {
        sCurrentCP = CASTLE_STARTING_CP;
        sCurrentStreak = 0;
        return;
    }

    GetCastleMonResults(party, results);
    sCurrentCP += CalculateCastlePoints(results, totalPPUsed, opponentLevelsRaised);
    sCurrentStreak++;
    if (sCurrentStreak > gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_50])
        gSaveBlock2Ptr->frontier.castleWinStreaks[FRONTIER_LVL_50] = sCurrentStreak;
    if (sCurrentStreak > gSaveBlock2Ptr->frontier.castleRecordWinStreaks[FRONTIER_LVL_50])
        gSaveBlock2Ptr->frontier.castleRecordWinStreaks[FRONTIER_LVL_50] = sCurrentStreak;
}

u16 Castle_GetWinStreak(u8 lvlMode)
{
    return gSaveBlock2Ptr->frontier.castleWinStreaks[lvlMode];
}
