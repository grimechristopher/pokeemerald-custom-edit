#include "global.h"
#include "battle_hall.h"
#include "pokemon.h"
#include "random.h"

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

static bool8 SpeciesIsEligible(u16 species, enum Type type, u8 rank)
{
    if (gSpeciesInfo[species].baseHP == 0)
        return FALSE; // not a real/enabled species
    if (gSpeciesInfo[species].isFrontierBanned)
        return FALSE;
    if (gSpeciesInfo[species].types[0] != type && gSpeciesInfo[species].types[1] != type)
        return FALSE;
    if (!IsBstAvailableAtRank(GetSpeciesBST(species), rank))
        return FALSE;

    return TRUE;
}

u32 CountHallEligibleSpecies(enum Type type, u8 rank)
{
    u32 species;
    u32 count = 0;

    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (SpeciesIsEligible(species, type, rank))
            count++;
    }

    return count;
}

u16 GetNthHallEligibleSpecies(enum Type type, u8 rank, u32 n)
{
    u32 species;
    u32 seen = 0;

    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (SpeciesIsEligible(species, type, rank))
        {
            if (seen == n)
                return species;
            seen++;
        }
    }

    return SPECIES_NONE;
}

u16 GetHallOpponentSpecies(enum Type type, u8 rank)
{
    u32 count = CountHallEligibleSpecies(type, rank);

    if (count == 0)
        return SPECIES_NONE;

    return GetNthHallEligibleSpecies(type, rank, Random() % count);
}
