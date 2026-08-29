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

#endif // GUARD_BATTLE_CASTLE_H
