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
