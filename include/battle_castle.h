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

// Battle Castle is an ordinary singles facility (3 mons/team per the design spec) — use
// FRONTIER_PARTY_SIZE, not MAX_FRONTIER_PARTY_SIZE (4, sized for the doubles format Castle
// doesn't use), or this would score a nonexistent 4th party slot.
u32 CalculateCastlePoints(const struct CastleMonResult mons[FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised);

// Not const: matches GetMonData's own non-const struct Pokemon * signature (the codebase's
// established idiom — no sibling Frontier facility uses a const-qualified party parameter).
void GetCastleMonResults(struct Pokemon party[FRONTIER_PARTY_SIZE], struct CastleMonResult results[FRONTIER_PARTY_SIZE]);

bool8 CastleShop_TrySpend(u32 *cp, u32 cost);

void Castle_StartChallenge(void);
u32 Castle_GetCurrentCP(void);
bool8 Castle_TrySpendCP(u32 cost);
void Castle_ApplyBattleResult(bool8 won, struct Pokemon party[FRONTIER_PARTY_SIZE], u32 totalPPUsed, u32 opponentLevelsRaised);
u16 Castle_GetWinStreak(u8 lvlMode);

#endif // GUARD_BATTLE_CASTLE_H
