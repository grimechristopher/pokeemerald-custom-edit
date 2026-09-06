# Tohjo Land Wild Encounters (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Populate `land_mons` wild encounter tables (Morning/Day/Evening/Night) for all 26 Kanto route maps and all 23 Johto route maps in `src/data/wild_encounters.json`, adapted from real HeartGold/SoulSilver data with deliberate Johto↔Kanto crossover.

**Architecture:** A small Python package (`tools/wild_encounters/tohjo/`) holds (1) an authored data module mapping each route to a 5-tier rarity table for Day and Night only, (2) pure functions that expand a tier table into this hack's fixed 12-slot format, derive Morning (=Day) and Evening (blended, not a copy) automatically, and apply a Johto-only level boost, and (3) a merge script that upserts the expanded result into `src/data/wild_encounters.json` without disturbing unrelated content, then regenerates `wild_encounters.h` via the repo's existing generator.

**Tech Stack:** Python 3, pytest (already available in this environment — verified `pytest 9.1.0`), the repo's existing `tools/wild_encounters/wild_encounters_to_header.py`.

**Design spec:** `docs/superpowers/specs/2026-09-05-tohjo-land-encounters-design.md`

---

## Background facts locked in during design (do not re-derive)

- `land_mons` is a fixed 12-slot table, rates `[20,20,10,10,10,10,5,5,4,4,1,1]`, shared by every map in this codebase. This groups into 5 tiers: **very_common** (slots 0-1, 2 slots), **common** (slots 2-5, 4 slots), **uncommon** (slots 6-7, 2 slots), **rare** (slots 8-9, 2 slots), **very_rare** (slots 10-11, 2 slots).
- `json.load` + `json.dump(..., indent=2)` on `src/data/wild_encounters.json` round-trips byte-identical (verified). This makes a script-based edit safe — it will not reformat unrelated parts of the file.
- Only one relevant group exists: `wild_encounter_groups` entry with `"label": "gWildMonHeaders"` (935 encounter entries today). The other two groups (`gBattlePyramidWildMonHeaders`, `gBattlePikeWildMonHeaders`) are untouched by this work.
- Bulbapedia URL convention (verified live): Kanto routes need the `Kanto_Route_N` form (e.g. `https://bulbapedia.bulbagarden.net/wiki/Kanto_Route_1`) because the plain `Route_N` title is a cross-region disambiguation page. Johto routes 26-48 are unambiguous and use the plain form (e.g. `https://bulbapedia.bulbagarden.net/wiki/Route_29`).
- Worked example already fetched and verified (use as the canonical pattern for every content task):
  - **Kanto Route 1** (HGSS): Day/Morning — Pidgey 2-4 (45%), Rattata 2 (30%), Sentret 3 (20%), Furret 6 (5%). Night — Rattata 2-3,6 (55%), Hoothoot 2-4 (45%).
  - **Johto Route 29** (HGSS): Day — Sentret 2-3 (40%), Pidgey 2-4 (55%), Rattata 4 (5%). Night — Hoothoot 2-4 (85%), Rattata 2-4 (15%).
- **Tier assignment rule:** rank the source species for a given time period by descending rate; rank 1 → `very_common`, rank 2 → `common`, rank 3 → `uncommon`, rank 4 → `rare`, rank 5 → `very_rare`. When the source lists fewer than 5 distinct species (the norm — Kanto Route 1 Day only has 4), the unfilled trailing tier(s) are exactly where a deliberate crossover pick goes. Crossover never overwrites a filled `very_common`/`common` tier.
- **Crossover asymmetry:** Johto species get the wider spread into Kanto than the reverse (per spec).
- **Named crossover requirements** (from the player, verified against real HGSS data so we know which are native-vs-import):
  - Houndour is Kanto-native (Route 7, night, verified) — the player specifically wants it ALSO on the Johto route between Goldenrod and Ecruteak (Route 36 or 37) as a genuine crossover import, at night.
  - Murkrow is Kanto-native (Routes 7 and 16, night, verified) — the player wants it more broadly present in Johto too; add it as a night crossover pick on 2-3 Johto routes (chunks F/G).
  - Slugma is NOT a wild HGSS encounter anywhere (egg-only in vanilla HGSS, verified) — the player wants it added as a homage pick near Mahogany Town/Lake of Rage (Route 42-44, chunk H), flagged as non-canon-sourced.
  - Larvitar (Safari Zone/Mt Silver Cave only, verified) and Misdreavus (Burned Tower only) have **no valid route placement** — both stay deferred to the future Johto-dungeons phase. Do not place them on any route map in this plan.
- **Johto level boost:** apply a `JOHTO_LEVEL_BOOST = 1.3` multiplier (round to nearest int, minimum +1 over the unboosted value) to every Johto route's min/max levels, applied once centrally in the expansion code — never hand-adjusted per route. This is the concrete form of "each region keeps its own curve, improve Johto's."

---

## File structure

- `tools/wild_encounters/tohjo/__init__.py` — empty, makes this a package.
- `tools/wild_encounters/tohjo/tiers.py` — pure functions: `expand_tier_table`, `derive_evening`, `resolve_morning`, constants `TIER_SLOT_ORDER` and `JOHTO_LEVEL_BOOST`.
- `tools/wild_encounters/tohjo/test_tiers.py` — pytest tests for the above.
- `tools/wild_encounters/tohjo/data.py` — authored `ROUTES` dict (Day + Night tier tables only, per route). This is the file that grows with each content task.
- `tools/wild_encounters/tohjo/validate_data.py` — CLI script: imports `data.ROUTES`, runs every route through `tiers.py`, prints `OK: N routes validated` or raises with the offending map ID. Used as the sanity check at the end of every content task.
- `tools/wild_encounters/tohjo/merge.py` — CLI script: expands `data.ROUTES` into wild_encounters.json's format and upserts into `src/data/wild_encounters.json`.
- `tools/wild_encounters/tohjo/test_merge.py` — pytest tests for `merge.py`, run against a small fixture JSON file (never the real 86k-line file).

---

### Task 1: Tier expansion, Evening derivation, and Johto level boost

**Files:**
- Create: `tools/wild_encounters/tohjo/__init__.py`
- Create: `tools/wild_encounters/tohjo/tiers.py`
- Test: `tools/wild_encounters/tohjo/test_tiers.py`

- [ ] **Step 1: Create the package init file**

```python
# tools/wild_encounters/tohjo/__init__.py
```

- [ ] **Step 2: Write the failing tests**

```python
# tools/wild_encounters/tohjo/test_tiers.py
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
```

- [ ] **Step 3: Run the tests to verify they fail**

Run: `cd tools/wild_encounters/tohjo && python3 -m pytest test_tiers.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'tiers'` (the module doesn't exist yet).

- [ ] **Step 4: Implement `tiers.py`**

```python
# tools/wild_encounters/tohjo/tiers.py
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
        if len(picked) == 1:
            evening["uncommon"] = picked
        else:
            # pad/truncate to exactly the uncommon slot count
            while len(picked) < TIER_SLOT_COUNTS["uncommon"]:
                picked.append(picked[-1])
            evening["uncommon"] = picked[:TIER_SLOT_COUNTS["uncommon"]]
    return evening
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `cd tools/wild_encounters/tohjo && python3 -m pytest test_tiers.py -v`
Expected: all 7 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tools/wild_encounters/tohjo/__init__.py tools/wild_encounters/tohjo/tiers.py tools/wild_encounters/tohjo/test_tiers.py
git commit -m "Add Tohjo land-encounter tier expansion, Evening derivation, and Johto level boost"
```

---

### Task 2: Merge script (upsert into wild_encounters.json)

**Files:**
- Create: `tools/wild_encounters/tohjo/merge.py`
- Test: `tools/wild_encounters/tohjo/test_merge.py`

- [ ] **Step 1: Write the failing tests**

```python
# tools/wild_encounters/tohjo/test_merge.py
import json
import os
import tempfile

from merge import merge_routes
from tiers import expand_tier_table

FIXTURE = {
    "wild_encounter_groups": [
        {
            "label": "gWildMonHeaders",
            "for_maps": True,
            "fields": [{"type": "land_mons", "encounter_rates": [20, 20, 10, 10, 10, 10, 5, 5, 4, 4, 1, 1]}],
            "encounters": [
                {
                    "map": "MAP_ROUTE29_JOHTO",
                    "base_label": "gJohtoRoute29_Morning",
                    "land_mons": {"encounter_rate": 20, "mons": [
                        {"min_level": 2, "max_level": 2, "species": "SPECIES_OLD_PLACEHOLDER"}
                    ] * 12},
                },
                {
                    "map": "MAP_ROUTE99_UNRELATED",
                    "base_label": "gRoute99_Morning",
                    "land_mons": {"encounter_rate": 20, "mons": [
                        {"min_level": 5, "max_level": 5, "species": "SPECIES_UNTOUCHED"}
                    ] * 12},
                },
            ],
        },
        {"label": "gBattlePyramidWildMonHeaders", "for_maps": False, "encounters": []},
    ]
}

ROUTES = {
    "MAP_ROUTE29_JOHTO": {
        "is_johto": True,
        "base_label_prefix": "gJohtoRoute29",
        "day": {
            "very_common": [("SPECIES_SENTRET", 2, 3)],
            "common": [("SPECIES_PIDGEY", 2, 4)],
            "uncommon": [("SPECIES_RATTATA", 4, 4)],
            "rare": [("SPECIES_RATTATA", 4, 4)],
            "very_rare": [("SPECIES_RATTATA", 4, 4)],
        },
        "night": {
            "very_common": [("SPECIES_HOOTHOOT", 2, 4)],
            "common": [("SPECIES_RATTATA", 2, 4)],
            "uncommon": [("SPECIES_RATTATA", 2, 4)],
            "rare": [("SPECIES_RATTATA", 2, 4)],
            "very_rare": [("SPECIES_RATTATA", 2, 4)],
        },
    },
}


def _write_fixture(tmp_path):
    path = os.path.join(tmp_path, "wild_encounters.json")
    with open(path, "w") as f:
        json.dump(FIXTURE, f, indent=2)
        f.write("\n")
    return path


def test_merge_replaces_existing_entry_for_all_four_times():
    with tempfile.TemporaryDirectory() as tmp:
        path = _write_fixture(tmp)
        merge_routes(path, ROUTES)
        with open(path) as f:
            result = json.load(f)
        group = next(g for g in result["wild_encounter_groups"] if g["label"] == "gWildMonHeaders")
        johto_entries = [e for e in group["encounters"] if e["map"] == "MAP_ROUTE29_JOHTO"]
        labels = {e["base_label"] for e in johto_entries}
        assert labels == {
            "gJohtoRoute29_Morning", "gJohtoRoute29_Day",
            "gJohtoRoute29_Evening", "gJohtoRoute29_Night",
        }
        morning = next(e for e in johto_entries if e["base_label"] == "gJohtoRoute29_Morning")
        assert morning["land_mons"]["mons"][0]["species"] == "SPECIES_SENTRET"
        assert "SPECIES_OLD_PLACEHOLDER" not in {m["species"] for m in morning["land_mons"]["mons"]}


def test_merge_leaves_unrelated_map_untouched():
    with tempfile.TemporaryDirectory() as tmp:
        path = _write_fixture(tmp)
        merge_routes(path, ROUTES)
        with open(path) as f:
            result = json.load(f)
        group = next(g for g in result["wild_encounter_groups"] if g["label"] == "gWildMonHeaders")
        unrelated = next(e for e in group["encounters"] if e["map"] == "MAP_ROUTE99_UNRELATED")
        assert unrelated["land_mons"]["mons"][0]["species"] == "SPECIES_UNTOUCHED"


def test_merge_is_idempotent():
    with tempfile.TemporaryDirectory() as tmp:
        path = _write_fixture(tmp)
        merge_routes(path, ROUTES)
        merge_routes(path, ROUTES)
        with open(path) as f:
            result = json.load(f)
        group = next(g for g in result["wild_encounter_groups"] if g["label"] == "gWildMonHeaders")
        johto_entries = [e for e in group["encounters"] if e["map"] == "MAP_ROUTE29_JOHTO"]
        assert len(johto_entries) == 4


def test_merge_inserts_a_brand_new_map_entry():
    new_routes = dict(ROUTES)
    new_routes["MAP_ROUTE1_FRLG"] = {
        "is_johto": False,
        "base_label_prefix": "gKantoRoute1",
        "day": ROUTES["MAP_ROUTE29_JOHTO"]["day"],
        "night": ROUTES["MAP_ROUTE29_JOHTO"]["night"],
    }
    with tempfile.TemporaryDirectory() as tmp:
        path = _write_fixture(tmp)
        merge_routes(path, new_routes)
        with open(path) as f:
            result = json.load(f)
        group = next(g for g in result["wild_encounter_groups"] if g["label"] == "gWildMonHeaders")
        kanto_entries = [e for e in group["encounters"] if e["map"] == "MAP_ROUTE1_FRLG"]
        assert len(kanto_entries) == 4
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cd tools/wild_encounters/tohjo && python3 -m pytest test_merge.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'merge'`.

- [ ] **Step 3: Implement `merge.py`**

```python
# tools/wild_encounters/tohjo/merge.py
"""Upserts Tohjo Phase 1 land encounter data into src/data/wild_encounters.json."""
import json
import sys

from tiers import expand_tier_table, resolve_morning, derive_evening

TIME_SUFFIXES = ["Morning", "Day", "Evening", "Night"]


def _build_entries(map_id, route):
    day = route["day"]
    night = route["night"]
    tables = {
        "Morning": resolve_morning(day),
        "Day": day,
        "Evening": derive_evening(day, night),
        "Night": night,
    }
    is_johto = route["is_johto"]
    prefix = route["base_label_prefix"]
    entries = []
    for suffix in TIME_SUFFIXES:
        slots = expand_tier_table(tables[suffix], is_johto)
        entries.append({
            "map": map_id,
            "base_label": f"{prefix}_{suffix}",
            "land_mons": {
                "encounter_rate": 20,
                "mons": [
                    {"min_level": s["min_level"], "max_level": s["max_level"], "species": s["species"]}
                    for s in slots
                ],
            },
        })
    return entries


def merge_routes(wild_encounters_json_path, routes):
    with open(wild_encounters_json_path) as f:
        data = json.load(f)

    group = next(g for g in data["wild_encounter_groups"] if g["label"] == "gWildMonHeaders")
    encounters = group["encounters"]

    for map_id, route in routes.items():
        # Drop any existing entries for this map (replace-in-place for Johto stubs,
        # no-op for brand-new Kanto maps), remembering the first index so new
        # entries land where the old ones were instead of at the end of the file.
        insert_at = len(encounters)
        kept = []
        removed_index = None
        for i, entry in enumerate(encounters):
            if entry.get("map") == map_id:
                if removed_index is None:
                    removed_index = i
            else:
                kept.append(entry)
        encounters = kept
        if removed_index is not None:
            insert_at = removed_index

        new_entries = _build_entries(map_id, route)
        encounters = encounters[:insert_at] + new_entries + encounters[insert_at:]

    group["encounters"] = encounters

    with open(wild_encounters_json_path, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


if __name__ == "__main__":
    from data import ROUTES
    path = sys.argv[1] if len(sys.argv) > 1 else "../../../src/data/wild_encounters.json"
    merge_routes(path, ROUTES)
    print(f"Merged {len(ROUTES)} routes into {path}")
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cd tools/wild_encounters/tohjo && python3 -m pytest test_merge.py -v`
Expected: all 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/wild_encounters/tohjo/merge.py tools/wild_encounters/tohjo/test_merge.py
git commit -m "Add Tohjo wild_encounters.json merge script with fixture tests"
```

---

### Task 3: Author `data.py` scaffold + validator + Kanto Routes 1-6

**Files:**
- Create: `tools/wild_encounters/tohjo/data.py`
- Create: `tools/wild_encounters/tohjo/validate_data.py`
- Modify: `tools/wild_encounters/tohjo/data.py` (add 6 routes)

- [ ] **Step 1: Create the scaffold**

```python
# tools/wild_encounters/tohjo/data.py
"""Authored Tohjo Phase 1 land encounter data.

Each entry maps a MAP_ constant to:
  is_johto: bool — whether the Johto level boost applies
  base_label_prefix: str — used to build "<prefix>_Morning" etc.
  day / night: tier tables (see tiers.py's module docstring for the format)

Only Day and Night are authored by hand. Morning and Evening are derived
mechanically by tiers.py (Morning = Day; Evening = a blend, never a copy).

Crossover picks (a species native to the other region) are marked inline
with a "# crossover: <reason>" comment. Non-canon homage picks (species
that HGSS never puts in the wild anywhere near here) are marked
"# homage, not vanilla-sourced: <reason>".
"""

ROUTES = {}
```

```python
# tools/wild_encounters/tohjo/validate_data.py
"""Sanity-checks every authored route in data.py. Run after adding routes."""
import sys

from data import ROUTES
from tiers import expand_tier_table


def main():
    for map_id, route in ROUTES.items():
        for key in ("is_johto", "base_label_prefix", "day", "night"):
            if key not in route:
                print(f"FAIL: {map_id} missing '{key}'")
                sys.exit(1)
        try:
            expand_tier_table(route["day"], route["is_johto"])
            expand_tier_table(route["night"], route["is_johto"])
        except ValueError as e:
            print(f"FAIL: {map_id}: {e}")
            sys.exit(1)
    print(f"OK: {len(ROUTES)} routes validated")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run the validator on the empty scaffold**

Run: `cd tools/wild_encounters/tohjo && python3 validate_data.py`
Expected: `OK: 0 routes validated`

- [ ] **Step 3: Author Kanto Routes 1-6**

For each of `MAP_ROUTE1_FRLG` through `MAP_ROUTE6_FRLG`, fetch the HGSS grass-encounter data from `https://bulbapedia.bulbagarden.net/wiki/Kanto_Route_N` (N = 1..6) and apply the tier-assignment rule from the Background section above (rank by rate, rank 1-5 → very_common..very_rare, unfilled trailing tiers are open for crossover). `MAP_ROUTE1_FRLG`'s worked example is already given in full in the Background section — add it to `ROUTES` exactly as shown there (as tuples, not the dict form used in the tiers.py tests):

```python
ROUTES["MAP_ROUTE1_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRoute1",
    "day": {
        "very_common": [("SPECIES_PIDGEY", 2, 4)],
        "common": [("SPECIES_RATTATA", 2, 2)],
        "uncommon": [("SPECIES_SENTRET", 3, 3)],
        "rare": [("SPECIES_FURRET", 6, 6)],
        "very_rare": [("SPECIES_HOOTHOOT", 2, 4)],  # crossover: Johto import, per spec's wider-Johto-spread rule
    },
    "night": {
        "very_common": [("SPECIES_RATTATA", 2, 3), ("SPECIES_RATTATA", 6, 6)],
        "common": [("SPECIES_HOOTHOOT", 2, 4)],
        "uncommon": [("SPECIES_SENTRET", 3, 3)],
        "rare": [("SPECIES_MURKROW", 10, 12)],  # crossover: Kanto-native (Route 7/16) given wider presence per player request
        "very_rare": [("SPECIES_HOUNDOUR", 8, 10)],  # crossover: Kanto-native (Route 7) given wider presence
    },
}
```

Repeat the fetch → rank → tier-assign procedure for Routes 2 through 6, using real Bulbapedia data for the native `very_common`/`common`/`uncommon` tiers and your judgment for any open `rare`/`very_rare` crossover slots (Johto species preferred, per the asymmetry rule). Add each as its own `ROUTES["MAP_ROUTEn_FRLG"] = {...}` block, same shape as above.

- [ ] **Step 4: Validate**

Run: `cd tools/wild_encounters/tohjo && python3 validate_data.py`
Expected: `OK: 6 routes validated`

- [ ] **Step 5: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py tools/wild_encounters/tohjo/validate_data.py
git commit -m "Author Tohjo land encounters for Kanto Routes 1-6"
```

---

### Task 4: Kanto Routes 7-12

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Kanto_Route_N` for N = 7..12. Apply the same tier-assignment rule. Route 7 is where Houndour and Murkrow are natively found (night) per the Background section — place them as native `very_common`/`common`-or-lower tiers there according to their actual listed rates, not as crossover (they belong here). Route 16 is Murkrow's other native route — same treatment.
- [ ] **Step 2:** Add each using this shape (same as Task 3's `MAP_ROUTE1_FRLG` example — `is_johto` stays `False` for every Kanto route, no level boost applies):

```python
ROUTES["MAP_ROUTEn_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRouteN",
    "day": {
        "very_common": [(...)],
        "common": [(...)],
        "uncommon": [(...)],
        "rare": [(...)],
        "very_rare": [(...)],
    },
    "night": {
        "very_common": [(...)],
        "common": [(...)],
        "uncommon": [(...)],
        "rare": [(...)],
        "very_rare": [(...)],
    },
}
```
- [ ] **Step 3:** Run: `cd tools/wild_encounters/tohjo && python3 validate_data.py` — Expected: `OK: 12 routes validated`
- [ ] **Step 4: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Kanto Routes 7-12"
```

---

### Task 5: Kanto Routes 13-18

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Kanto_Route_N` for N = 13..18. Same tier-assignment rule, Johto crossover preferred for open rare/very_rare slots.
- [ ] **Step 2:** Add each using the same shape shown in Task 4 (`is_johto: False`, 5 tiers under `day` and `night`).
- [ ] **Step 3:** Run: `python3 validate_data.py` — Expected: `OK: 18 routes validated`
- [ ] **Step 4: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Kanto Routes 13-18"
```

---

### Task 6: Kanto Routes 19, 20, 21 North, 21 South, 22, 23

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Kanto_Route_N` for N = 19, 20, 22, 23, plus the specific Route 21 North/South pages (search Bulbapedia for "Kanto Route 21" if a single page covers both halves — split its data sensibly between `MAP_ROUTE21_NORTH_FRLG` and `MAP_ROUTE21_SOUTH_FRLG`). These routes run along Cinnabar Island's approach.
- [ ] **Step 2:** Slugma is not a valid native pick here (verified: HGSS never puts it in the wild) — do NOT add it in this task. It belongs near Mahogany Town in Johto (Task 10), not Kanto.
- [ ] **Step 3:** Add each as `ROUTES["MAP_ROUTEn_FRLG"]` (map IDs: `MAP_ROUTE19_FRLG`, `MAP_ROUTE20_FRLG`, `MAP_ROUTE21_NORTH_FRLG`, `MAP_ROUTE21_SOUTH_FRLG`, `MAP_ROUTE22_FRLG`, `MAP_ROUTE23_FRLG`).
- [ ] **Step 4:** Run: `python3 validate_data.py` — Expected: `OK: 24 routes validated`
- [ ] **Step 5: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Kanto Route 19-23 (incl. Route 21 N/S)"
```

---

### Task 7: Kanto Routes 24-25 + Johto Routes 26-29

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Kanto_Route_24` and `_25`. Add as `MAP_ROUTE24_FRLG`, `MAP_ROUTE25_FRLG`.
- [ ] **Step 2:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Route_26` through `Route_29` (Johto — no prefix needed, verified unambiguous). `Route_29`'s data is already given in full in the Background section as the worked example — use it directly (with `is_johto: True` so the level boost applies):

```python
ROUTES["MAP_ROUTE29_JOHTO"] = {
    "is_johto": True,
    "base_label_prefix": "gJohtoRoute29",
    "day": {
        "very_common": [("SPECIES_PIDGEY", 2, 4)],
        "common": [("SPECIES_SENTRET", 2, 3)],
        "uncommon": [("SPECIES_RATTATA", 4, 4)],
        "rare": [("SPECIES_RATTATA", 4, 4)],
        "very_rare": [("SPECIES_RATTATA", 4, 4)],  # open native tiers filled conservatively; adjust if a better crossover fits
    },
    "night": {
        "very_common": [("SPECIES_HOOTHOOT", 2, 4)],
        "common": [("SPECIES_RATTATA", 2, 4)],
        "uncommon": [("SPECIES_RATTATA", 2, 4)],
        "rare": [("SPECIES_RATTATA", 2, 4)],
        "very_rare": [("SPECIES_RATTATA", 2, 4)],
    },
}
```

Note this is intentionally close to the pre-existing seed data already in `wild_encounters.json` for this map (Sentret/Pidgey/Rattata/Hoothoot) — the merge script will replace those 4 old entries with the tier-expanded version, which is expected and correct (Task 11 does the actual replace).

- [ ] **Step 3:** Add Routes 26, 27, 28 similarly from their fetched data.
- [ ] **Step 4:** Run: `python3 validate_data.py` — Expected: `OK: 30 routes validated`
- [ ] **Step 5: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Kanto Routes 24-25 and Johto Routes 26-29"
```

---

### Task 8: Johto Routes 30-35

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Route_N` for N = 30..35. Same tier-assignment rule; Kanto crossover here should be sparser than Johto's crossover into Kanto (per the asymmetry rule) — most open tiers should stay Johto-native or go to a thematically strong Kanto pick, not filled reflexively.
- [ ] **Step 2:** Add each as `ROUTES["MAP_ROUTEn_JOHTO"] = {...}` with `is_johto: True`.
- [ ] **Step 3:** Run: `python3 validate_data.py` — Expected: `OK: 36 routes validated`
- [ ] **Step 4: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Johto Routes 30-35"
```

---

### Task 9: Johto Routes 36-41

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Route_N` for N = 36..41. Route 36 and/or 37 form the Goldenrod–Ecruteak connection — this is where the player explicitly wants Houndour added as a night crossover pick (it is not HGSS-native here; add it deliberately to `rare` or `very_rare` at night with a `# crossover, player-requested: Goldenrod-Ecruteak night pick` comment). Also add Murkrow as a night crossover pick on one of these routes (`# crossover: wider Johto presence per player request`).
- [ ] **Step 2:** Add each as `ROUTES["MAP_ROUTEn_JOHTO"]`.
- [ ] **Step 3:** Run: `python3 validate_data.py` — Expected: `OK: 42 routes validated`
- [ ] **Step 4: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Johto Routes 36-41 (incl. Houndour/Murkrow crossover)"
```

---

### Task 10: Johto Routes 42-48

**Files:** Modify: `tools/wild_encounters/tohjo/data.py`

- [ ] **Step 1:** Fetch `https://bulbapedia.bulbagarden.net/wiki/Route_N` for N = 42..48. These routes run through the Mahogany Town / Lake of Rage / Blackthorn area. This is where the player wants Slugma added as a homage pick (verified non-canon — HGSS never places it in the wild) — add it to a `rare` or `very_rare` tier on whichever of these routes sits closest to Mahogany Town, with a `# homage, not vanilla-sourced: player-requested Team Rocket/Mahogany flavor` comment.
- [ ] **Step 2:** Do NOT add Larvitar or Misdreavus anywhere in this task (verified: both are dungeon-only in HGSS — Mt Silver Cave/Safari Zone and Burned Tower respectively — and those dungeons don't exist as maps yet; they're deferred to the future Johto-dungeons phase per the design spec).
- [ ] **Step 3:** Add each as `ROUTES["MAP_ROUTEn_JOHTO"]`.
- [ ] **Step 4:** Run: `python3 validate_data.py` — Expected: `OK: 49 routes validated`
- [ ] **Step 5: Commit**

```bash
git add tools/wild_encounters/tohjo/data.py
git commit -m "Author Tohjo land encounters for Johto Routes 42-48 (incl. Slugma homage pick)"
```

---

### Task 11: Merge into the real file and verify the build

**Files:**
- Modify: `src/data/wild_encounters.json`
- Generated: `src/data/wild_encounters.h`

- [ ] **Step 1: Confirm all 49 routes are authored**

Run: `cd tools/wild_encounters/tohjo && python3 validate_data.py`
Expected: `OK: 49 routes validated`

- [ ] **Step 2: Run the merge against the real file**

Run: `cd tools/wild_encounters/tohjo && python3 merge.py ../../../src/data/wild_encounters.json`
Expected: `Merged 49 routes into ../../../src/data/wild_encounters.json`

- [ ] **Step 3: Confirm the JSON is still valid and only the intended maps changed**

Run:
```bash
python3 -c "import json; json.load(open('src/data/wild_encounters.json'))" && echo "valid JSON"
git diff --stat src/data/wild_encounters.json
```
Expected: `valid JSON`, and the diff stat shows only `src/data/wild_encounters.json` changed (the merge script never touches other files).

- [ ] **Step 4: Regenerate the header and build**

Run: `make -j$(nproc)`
Expected: build completes with no errors. This regenerates `src/data/wild_encounters.h` via the existing `tools/wild_encounters/wild_encounters_to_header.py` (wired into the Makefile already) as part of the normal build.

- [ ] **Step 5: Commit**

```bash
git add src/data/wild_encounters.json src/data/wild_encounters.h
git commit -m "Merge Tohjo Phase 1 land encounters into wild_encounters.json"
```
