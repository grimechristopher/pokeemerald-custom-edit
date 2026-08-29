#include "global.h"
#include "event_data.h"
#include "test/battle.h"

#if B_VAR_STARTING_STATUS != 0

SINGLE_BATTLE_TEST("B_VAR_STARTING_STATUS starts a chosen weather at the beginning of battle")
{
    u16 weather;

    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_SUN; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_RAIN; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_SANDSTORM; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_HAIL; }
    PARAMETRIZE { weather = STARTING_STATUS_WEATHER_FOG; }

    VarSet(B_VAR_STARTING_STATUS, weather);
    VarSet(B_VAR_STARTING_STATUS_TIMER, 0);

    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { ; }
    } SCENE {
        switch (weather)
        {
        case STARTING_STATUS_WEATHER_SUN:
            MESSAGE("The sunlight is harsh!");
            ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_SUN_CONTINUES);
            break;
        case STARTING_STATUS_WEATHER_RAIN:
            MESSAGE("It's raining!");
            ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_RAIN_CONTINUES);
            break;
        case STARTING_STATUS_WEATHER_SANDSTORM:
            MESSAGE("The sandstorm is raging!");
            ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_SANDSTORM_CONTINUES);
            break;
        case STARTING_STATUS_WEATHER_HAIL:
            if (B_OVERWORLD_SNOW >= GEN_9)
            {
                MESSAGE("It's snowing!");
                ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_SNOW_CONTINUES);
            }
            else
            {
                MESSAGE("It's hailing!");
                ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_HAIL_CONTINUES);
            }
            break;
        case STARTING_STATUS_WEATHER_FOG:
            MESSAGE("The fog is deep…");
            ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FOG_CONTINUES);
            break;
        }
    } THEN {
        VarSet(B_VAR_STARTING_STATUS, 0);
    }
}

#endif // B_VAR_STARTING_STATUS
