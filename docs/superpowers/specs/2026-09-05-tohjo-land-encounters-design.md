# Tohjo Land Wild Encounters — Phase 1 Design

**Date:** 2026-09-05
**Status:** Design Approved, Ready for Implementation Planning
**Parent vision:** `docs/plans/2025-12-20-four-region-game-design.md` (four-region game: Hoenn/Johto/Kanto/Sinnoh). This spec covers the Johto+Kanto "Tohjo" wild-encounter slice of that vision.

---

## Context

The player's fan game lets a run start in either **Pallet Town** (Kanto) or **New Bark Town** (Johto), with Johto and Kanto treated as one interconnected "Tohjo" region (à la HGSS's Johto/Kanto duology, but with two valid starting points and two-way species crossover).

Current repo state (verified 2026-09-05):
- **Kanto**: fully real map geometry — every FRLG map (towns, routes, dungeons, Sevii Islands) exists with real layouts and connections. Wild encounters are almost entirely unpopulated (only Safari Zone North and Victory Road 1F have any data).
- **Johto**: skeleton only — Routes 26–48 exist as map entries (23 maps) sharing one placeholder layout (`LAYOUT_JOHTO_ROUTE_STUB`), `connections: null`, `region_map_section: MAPSEC_DYNAMIC`. `land_mons` is already partially seeded across all 4 time-of-day labels for most of these routes (real HGSS-style data — Sentret/Pidgey/Rattata/Hoothoot etc.), but only 3 routes have any `water_mons`, and nothing else. No Johto towns or dungeons (New Bark Town, Violet City, Union Cave, Ilex Forest, Mt Mortar, Dark Cave, Burned Tower, etc.) exist as maps yet.

**Map naming:** the player's new maps going forward will follow a `Region_RouteN` convention. This does **not** apply to anything already imported from Emerald or FRLG, nor to the existing Johto route stubs — none of those get renamed as part of this work. The convention only governs genuinely new maps built from here on (e.g., a future real New Bark Town).

## Scope of this phase

**In scope:** `land_mons` encounter tables (all 4 time-of-day slots: Morning/Day/Evening/Night) for every existing route map:
- **Kanto (26 maps):** `Route1_Frlg` … `Route20_Frlg`, `Route22_Frlg` … `Route25_Frlg`, `Route21_North_Frlg`, `Route21_South_Frlg`
- **Johto (23 maps):** `Route26_Johto` … `Route48_Johto`

**Out of scope, deferred to later phases (not blocking this one):**
- Surf / Fishing (old/good/super rod) / Rock Smash / Headbutt tables — own follow-up passes once the land roster (species pool per route) is settled, since those tables reuse the same per-route "who lives here" decisions.
- **Honey Trees** — not an implemented mechanic in this codebase at all (no field object, no interaction script, no `honey_mons` type in the JSON schema or `wild_encounters_to_header.py`). Needs its own feature-design+implementation pass before any table work.
- **Wild-level scaling** — not implemented. Would be a runtime multiplier in `wild_encounter.c` layered on top of the base `min_level`/`max_level` values this phase produces; nothing here needs to change to support it later.
- Johto towns/dungeons that don't exist as maps yet (Mt Mortar, Dark Cave, Burned Tower, Union Cave, Ilex Forest, etc.) — species the player specifically wants there (Larvitar, Misdreavus, etc.) get placed on the correct **routes** where HGSS also puts them, but dungeon-exclusive placements wait until those maps exist.
- Renaming any existing map.

## Rarity-tier model

This hack's `land_mons` field is a fixed 12-slot table with rates `[20, 20, 10, 10, 10, 10, 5, 5, 4, 4, 1, 1]` (declared once, shared by every map). That groups into 5 tiers:

| Tier | Slots | Total weight |
|---|---|---|
| Very Common | 2 | 40% |
| Common | 4 | 40% |
| Uncommon | 2 | 10% |
| Rare | 2 | 8% |
| Very Rare | 2 | 2% |

Default: one species per tier, filling all slots in that tier. A tier is split into two different species only where canon clearly wants it (e.g., two iconic common birds sharing a route's Common tier). This is an authoring simplification, not a hard rule — call it out explicitly wherever a route's real HGSS/FRLG table meaningfully splits a tier.

## Species pool & source

- **Kanto land:** HGSS's actual Kanto route tables (Kanto post-national-dex in HGSS has real Day/Night data — not FRLG's static single table, which pre-dates the day/night engine).
- **Johto land:** HGSS's actual Johto route tables.

Both regions come from the same source game's real time-of-day data. Confidence is high for iconic/heavily-documented routes; less-traveled midgame routes get an explicit low-confidence flag in the authored table so they can be spot-checked before merge.

## Time-of-day mapping

This hack's schema has 4 slots (Morning/Day/Evening/Night); HGSS itself only distinguishes Day vs. Night. Mapping:

- **Morning = Day** (reuses Day's table verbatim — matches the existing Route 29 seed data pattern).
- **Day** = HGSS's Day table.
- **Evening = its own transitional table, not a copy of Day or Night.** Day's Very Common/Common tiers stay mostly intact (dusk hasn't changed the route's core character yet), but nocturnal species get introduced early, promoted one tier down from where they sit at Night (e.g., a nocturnal pick that's Common at Night is Uncommon at Evening, not absent).
- **Night** = HGSS's actual Night table.

## Crossover design

Per examples given (Houndour at night on the Goldenrod–Ecruteak route, Murkrow scattered through Johto routes, Slugma/Misdreavus placements, etc.): this is faithful HGSS/FRLG placement with genuine cross-region guest appearances layered in, not an abstract rarity formula.

- Crossover (a Johto species on a Kanto route, or vice versa) only occupies **Uncommon / Rare / Very Rare** tiers — it never displaces a route's native Very-Common/Common identity.
- Placement must be thematically justified (nocturnal → night/evening slot; volcanic → routes near Cinnabar; etc.), matching real examples like the ones given, not scattered randomly.
- **Asymmetric weighting:** Johto species get the wider spread — more Kanto routes carry a Johto guest than the reverse. Kanto species crossing into Johto routes should be comparatively sparser.

## Level curve

Each region keeps its own original shape (low near its own starting town, rising toward its own endgame) — so starting in Pallet or New Bark feels equally gentle. Johto's curve is nudged upward from HGSS's original numbers, which are unusually flat/low, so it doesn't feel undertuned sitting next to Kanto's.

## Delivery mechanism

1. Author the full route-by-route land table as a structured, human-readable data file (not hand-edited JSON) — grouped by route, with tier/species/level-range/time-of-day and inline notes for crossover picks and low-confidence entries.
2. Player reviews the authored table before anything touches the JSON.
3. Write a small merge script that expands the tier data into the existing 12-slot `land_mons` format and injects it into `src/data/wild_encounters.json`, following the exact structure already used by populated maps (e.g. Route101, Route119, the existing Route29_Johto seed data).
4. Regenerate `src/data/wild_encounters.h` via the repo's existing `tools/wild_encounters/wild_encounters_to_header.py` (already wired into the Makefile).
5. Confirm the project builds (`make`) with no errors introduced.

## Success criteria

- [ ] All 26 Kanto route maps have `land_mons` entries for Morning/Day/Evening/Night.
- [ ] All 23 Johto route maps have `land_mons` entries for Morning/Day/Evening/Night (extending/replacing the existing partial seed data where needed).
- [ ] Crossover placements match the tier rule (Uncommon/Rare/Very-Rare only) and include the player's example species in their correct locations.
- [ ] Johto curve is audibly less flat than stock HGSS while Kanto's curve is left close to its FRLG/HGSS shape.
- [ ] `wild_encounters.h` regenerates cleanly and the project builds.
- [ ] Low-confidence entries are flagged in the authored table for the player's review, not silently guessed.
