#include "global.h"
#include "battle_hall.h"
#include "pokemon.h"

struct HallBstBracket
{
    u16 minBst;
    u16 maxBst;
    u8 minRank;
};

// BST brackets and their unlock rank, per the source games (Bulbapedia: "List of Battle Hall Pokémon").
// The top bracket has no documented upper bound, so it is left open (maxBst = 0xFFFF) rather than
// guessing a ceiling — the source material only specifies unlock ranks, not an upper cutoff.
static const struct HallBstBracket sHallBstBrackets[] =
{
    { 0,   339,    HALL_MIN_RANK },
    { 340, 439,    3 },
    { 440, 0xFFFF, 6 },
};

#define HALL_BST_BRACKET_COUNT (sizeof(sHallBstBrackets) / sizeof(sHallBstBrackets[0]))

bool8 IsBstAvailableAtRank(u32 bst, u8 rank)
{
    u32 i;

    for (i = 0; i < HALL_BST_BRACKET_COUNT; i++)
    {
        if (bst >= sHallBstBrackets[i].minBst && bst <= sHallBstBrackets[i].maxBst)
            return rank >= sHallBstBrackets[i].minRank;
    }

    return FALSE;
}

u32 GetSpeciesBST(u16 species)
{
    return GetSpeciesBaseStat(species, STAT_HP)
         + GetSpeciesBaseStat(species, STAT_ATK)
         + GetSpeciesBaseStat(species, STAT_DEF)
         + GetSpeciesBaseStat(species, STAT_SPEED)
         + GetSpeciesBaseStat(species, STAT_SPATK)
         + GetSpeciesBaseStat(species, STAT_SPDEF);
}
