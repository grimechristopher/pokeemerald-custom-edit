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
