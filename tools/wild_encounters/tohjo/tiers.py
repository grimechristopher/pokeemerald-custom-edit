"""Pure functions for expanding Tohjo Phase 1 land-encounter tier tables
into this hack's fixed 12-slot land_mons format.

A tier table is a dict with keys "very_common", "common", "uncommon", "rare",
"very_rare". Each value is a list of (species, min_level, max_level) tuples,
either length 1 (fills every slot in that tier) or exactly equal to the
tier's slot count.
"""

TIER_SLOT_COUNTS = {
    "very_common": 2,
    "common": 4,
    "uncommon": 2,
    "rare": 2,
    "very_rare": 2,
}

# Order matches the fixed land_mons rate curve [20,20,10,10,10,10,5,5,4,4,1,1]
TIER_SLOT_ORDER = ["very_common", "common", "uncommon", "rare", "very_rare"]

JOHTO_LEVEL_BOOST = 1.3


def _boost_level(level, is_johto):
    if not is_johto:
        return level
    boosted = round(level * JOHTO_LEVEL_BOOST)
    return max(boosted, level + 1)


def expand_tier_table(tiers, is_johto):
    """Expand a tier table into an ordered list of 12 {species, min_level, max_level} dicts."""
    slots = []
    for tier_name in TIER_SLOT_ORDER:
        slot_count = TIER_SLOT_COUNTS[tier_name]
        entries = tiers[tier_name]
        if len(entries) == 1:
            entries = entries * slot_count
        if len(entries) != slot_count:
            raise ValueError(
                f"tier '{tier_name}' has {len(entries)} entries, expected 1 or {slot_count}"
            )
        for species, min_level, max_level in entries:
            slots.append({
                "species": species,
                "min_level": _boost_level(min_level, is_johto),
                "max_level": _boost_level(max_level, is_johto),
            })
    return slots


def resolve_morning(day_tiers):
    """Morning always reuses Day's table verbatim."""
    return day_tiers


def derive_evening(day_tiers, night_tiers):
    """Evening keeps Day's very_common/common tiers, replaces uncommon with any
    night-exclusive species (promoted one tier earlier than their night slot),
    and keeps Day's rare/very_rare untouched. Falls back to Day's uncommon
    tier when there are no night-exclusive species."""
    day_species = {
        species
        for tier in day_tiers.values()
        for species, _, _ in tier
    }
    night_exclusive = []
    for tier_name in ("very_common", "common", "uncommon", "rare", "very_rare"):
        for entry in night_tiers[tier_name]:
            species = entry[0]
            if species not in day_species and species not in {e[0] for e in night_exclusive}:
                night_exclusive.append(entry)

    evening = dict(day_tiers)
    if night_exclusive:
        picked = night_exclusive[:TIER_SLOT_COUNTS["uncommon"]]
        while len(picked) < TIER_SLOT_COUNTS["uncommon"]:
            picked.append(picked[-1])
        evening["uncommon"] = picked
    return evening
