#include "global.h"
#include "test/test.h"
#include "battle_hall.h"

TEST("IsBstAvailableAtRank gates species by base stat total and rank")
{
    // BST < 340 is available from Rank 1
    EXPECT(IsBstAvailableAtRank(200, 1));
    EXPECT(IsBstAvailableAtRank(339, 1));

    // BST 340-439 only from Rank 3
    EXPECT(!(IsBstAvailableAtRank(400, 1)));
    EXPECT(!(IsBstAvailableAtRank(400, 2)));
    EXPECT(IsBstAvailableAtRank(400, 3));

    // BST 440+ only from Rank 6
    EXPECT(!(IsBstAvailableAtRank(450, 5)));
    EXPECT(IsBstAvailableAtRank(450, 6));
    EXPECT(IsBstAvailableAtRank(720, 10)); // very high BST still gated the same way, no upper bound
}

TEST("GetSpeciesBST sums all six base stats")
{
    u32 bst = GetSpeciesBST(SPECIES_WOBBUFFET);
    u32 expected = GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_HP)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_ATK)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_DEF)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_SPEED)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_SPATK)
                  + GetSpeciesBaseStat(SPECIES_WOBBUFFET, STAT_SPDEF);
    EXPECT_EQ(bst, expected);
}

TEST("GetNthHallEligibleSpecies only returns species of the requested type, not banned")
{
    u32 count = CountHallEligibleSpecies(TYPE_WATER, HALL_MAX_RANK);
    u32 i;

    EXPECT(count > 0);

    for (i = 0; i < count && i < 20; i++)
    {
        u16 species = GetNthHallEligibleSpecies(TYPE_WATER, HALL_MAX_RANK, i);
        bool8 isWaterType = (gSpeciesInfo[species].types[0] == TYPE_WATER
                           || gSpeciesInfo[species].types[1] == TYPE_WATER);
        EXPECT(isWaterType);
        EXPECT(!(gSpeciesInfo[species].isFrontierBanned));
    }
}

TEST("CountHallEligibleSpecies shrinks as rank drops (fewer high-BST mons unlocked)")
{
    u32 countAtMaxRank = CountHallEligibleSpecies(TYPE_NORMAL, HALL_MAX_RANK);
    u32 countAtRank1 = CountHallEligibleSpecies(TYPE_NORMAL, HALL_MIN_RANK);

    // Strictly less-than (not <=): the source games' Normal-type roster has enough high-BST
    // members gated behind higher ranks that this must be a real decrease, not just a tie —
    // a `<=` here would pass even if rank-gating regressed into a no-op.
    EXPECT(countAtRank1 < countAtMaxRank);
}

TEST("GetHallOpponentSpecies always returns a species matching the requested type")
{
    u32 i;

    for (i = 0; i < 20; i++)
    {
        u16 species = GetHallOpponentSpecies(TYPE_FIRE, HALL_MAX_RANK);
        bool8 isFireType = (gSpeciesInfo[species].types[0] == TYPE_FIRE
                          || gSpeciesInfo[species].types[1] == TYPE_FIRE);
        EXPECT(isFireType);
    }
}
