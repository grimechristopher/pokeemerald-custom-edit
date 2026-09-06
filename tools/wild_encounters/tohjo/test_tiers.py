from tiers import expand_tier_table, derive_evening, resolve_morning, JOHTO_LEVEL_BOOST

# A route's tier table: each tier maps to a list of (species, min_level, max_level) tuples.
# The list has either 1 entry (fills every slot in that tier) or exactly as many entries
# as the tier has slots (very_common=2, common=4, uncommon=2, rare=2, very_rare=2).
KANTO_ROUTE_1_DAY = {
    "very_common": [("SPECIES_PIDGEY", 2, 4)],
    "common": [("SPECIES_RATTATA", 2, 2)],
    "uncommon": [("SPECIES_SENTRET", 3, 3)],
    "rare": [("SPECIES_FURRET", 6, 6)],
    "very_rare": [("SPECIES_HOOTHOOT", 2, 4)],  # crossover: Johto import into an unfilled Kanto tier
}

KANTO_ROUTE_1_NIGHT = {
    "very_common": [("SPECIES_RATTATA", 2, 3), ("SPECIES_RATTATA", 6, 6)],
    "common": [("SPECIES_HOOTHOOT", 2, 4)],
    "uncommon": [("SPECIES_SENTRET", 3, 3)],
    "rare": [("SPECIES_MURKROW", 10, 12)],
    "very_rare": [("SPECIES_HOUNDOUR", 8, 10)],
}


def test_expand_tier_table_produces_12_ordered_slots():
    slots = expand_tier_table(KANTO_ROUTE_1_DAY, is_johto=False)
    assert len(slots) == 12
    # very_common fills slots 0-1 with the single given species
    assert slots[0] == {"species": "SPECIES_PIDGEY", "min_level": 2, "max_level": 4}
    assert slots[1] == {"species": "SPECIES_PIDGEY", "min_level": 2, "max_level": 4}
    # common fills slots 2-5
    assert slots[2] == {"species": "SPECIES_RATTATA", "min_level": 2, "max_level": 2}
    assert slots[5] == {"species": "SPECIES_RATTATA", "min_level": 2, "max_level": 2}
    # very_rare fills slots 10-11
    assert slots[10] == {"species": "SPECIES_HOOTHOOT", "min_level": 2, "max_level": 4}
    assert slots[11] == {"species": "SPECIES_HOOTHOOT", "min_level": 2, "max_level": 4}


def test_expand_tier_table_splits_a_tier_across_two_species():
    slots = expand_tier_table(KANTO_ROUTE_1_NIGHT, is_johto=False)
    # very_common has two distinct entries, one per slot
    assert slots[0] == {"species": "SPECIES_RATTATA", "min_level": 2, "max_level": 3}
    assert slots[1] == {"species": "SPECIES_RATTATA", "min_level": 6, "max_level": 6}


def test_expand_tier_table_rejects_wrong_slot_count():
    bad = dict(KANTO_ROUTE_1_DAY)
    bad["common"] = [("SPECIES_RATTATA", 2, 2), ("SPECIES_PIDGEY", 2, 2)]  # 2 entries, needs 1 or 4
    try:
        expand_tier_table(bad, is_johto=False)
        assert False, "expected a ValueError"
    except ValueError as e:
        assert "common" in str(e)


def test_johto_level_boost_multiplies_and_rounds_up_by_at_least_one():
    slots = expand_tier_table(KANTO_ROUTE_1_DAY, is_johto=True)
    # 2 * 1.3 = 2.6 -> round to 3; must be at least +1 over the unboosted value (2 -> 3)
    assert slots[0]["min_level"] == 3
    # rare tier (slots 8-9) holds Furret, max_level 6: 6 * 1.3 = 7.8 -> round to 8
    assert slots[9]["max_level"] == 8


def test_resolve_morning_reuses_day_table_unchanged():
    morning = resolve_morning(KANTO_ROUTE_1_DAY)
    assert morning == KANTO_ROUTE_1_DAY


def test_derive_evening_keeps_day_common_tiers_and_promotes_one_night_exclusive_species():
    evening = derive_evening(KANTO_ROUTE_1_DAY, KANTO_ROUTE_1_NIGHT)
    # Day's very_common/common carry over unchanged (dusk hasn't changed the route's core character)
    assert evening["very_common"] == KANTO_ROUTE_1_DAY["very_common"]
    assert evening["common"] == KANTO_ROUTE_1_DAY["common"]
    # uncommon is replaced by night-exclusive species (species in night but not anywhere in day),
    # promoted here one tier earlier than their night slot (rare/very_rare at night -> uncommon at evening)
    evening_uncommon_species = {s for s, _, _ in evening["uncommon"]}
    assert evening_uncommon_species == {"SPECIES_MURKROW", "SPECIES_HOUNDOUR"}
    # rare/very_rare stay as Day's (native rare crossover picks stay stable across day/evening)
    assert evening["rare"] == KANTO_ROUTE_1_DAY["rare"]
    assert evening["very_rare"] == KANTO_ROUTE_1_DAY["very_rare"]


def test_derive_evening_falls_back_to_day_uncommon_when_no_night_exclusive_species():
    same = {"very_common": [("SPECIES_PIDGEY", 2, 2)], "common": [("SPECIES_PIDGEY", 2, 2)],
            "uncommon": [("SPECIES_PIDGEY", 2, 2)], "rare": [("SPECIES_PIDGEY", 2, 2)],
            "very_rare": [("SPECIES_PIDGEY", 2, 2)]}
    evening = derive_evening(same, same)
    assert evening["uncommon"] == same["uncommon"]


def test_derive_evening_pads_a_single_night_exclusive_species_to_full_tier():
    day = {
        "very_common": [("SPECIES_PIDGEY", 2, 4)],
        "common": [("SPECIES_RATTATA", 2, 2)],
        "uncommon": [("SPECIES_SENTRET", 3, 3)],
        "rare": [("SPECIES_FURRET", 6, 6)],
        "very_rare": [("SPECIES_PIDGEY", 2, 4)],
    }
    night = dict(day)
    night["common"] = [("SPECIES_HOOTHOOT", 2, 4)]  # only one species not present anywhere in day
    evening = derive_evening(day, night)
    assert evening["uncommon"] == [("SPECIES_HOOTHOOT", 2, 4), ("SPECIES_HOOTHOOT", 2, 4)]
