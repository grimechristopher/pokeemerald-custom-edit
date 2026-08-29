#ifndef GUARD_BATTLE_HALL_H
#define GUARD_BATTLE_HALL_H

#define HALL_MIN_RANK 1
#define HALL_MAX_RANK 10

bool8 IsBstAvailableAtRank(u32 bst, u8 rank);
u32 GetSpeciesBST(u16 species);
u16 GetHallOpponentSpecies(enum Type type, u8 rank);
u32 CountHallEligibleSpecies(enum Type type, u8 rank);
u16 GetNthHallEligibleSpecies(enum Type type, u8 rank, u32 n);
u8 Hall_GetTypeRank(enum Type type);
void Hall_RecordBattleResult(enum Type type, bool8 won);

// TYPE_NONE means no debug Hall battle is in flight. Storing the actual type (rather than a
// separate bool8 flag + a type hardcoded independently at each call site) keeps the debug
// menu action and the CB2_EndTrainerBattle hook from being able to disagree about which
// type's rank a given debug battle's result should apply to.
extern enum Type gDebugHallBattleType;

#endif // GUARD_BATTLE_HALL_H
