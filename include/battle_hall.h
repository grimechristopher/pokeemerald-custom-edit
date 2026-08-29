#ifndef GUARD_BATTLE_HALL_H
#define GUARD_BATTLE_HALL_H

#define HALL_MIN_RANK 1
#define HALL_MAX_RANK 10

bool8 IsBstAvailableAtRank(u32 bst, u8 rank);
u32 GetSpeciesBST(u16 species);
u16 GetHallOpponentSpecies(enum Type type, u8 rank);
u32 CountHallEligibleSpecies(enum Type type, u8 rank);
u16 GetNthHallEligibleSpecies(enum Type type, u8 rank, u32 n);

#endif // GUARD_BATTLE_HALL_H
