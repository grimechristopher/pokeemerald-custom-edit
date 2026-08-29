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
