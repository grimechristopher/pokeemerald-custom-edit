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

# Route 2: Viridian Forest-adjacent grass, north (near the Forest) and south
# (near Diglett's Cave) sections share one encounter table in HGSS. Native
# HGSS data (union of HeartGold's Caterpie line and SoulSilver's Weedle line,
# plus the Johto bug/owl line HGSS itself added to Kanto) fills all 10 slots
# for both Day and Night, so there is no open tier for a crossover pick here
# — the route is already thoroughly Johto-flavored via Hoothoot/Noctowl/
# Spinarak/Ariados, which are native HGSS additions to this exact route, not
# imports of our own.
#
# Day ranking is by combined Morning+Day rate across both HG and SS (raw
# Bulbapedia wikitext, Northern section): Pidgey 140, Caterpie 62, Weedle 42,
# Kakuna 40, Ledyba 34 (SS-only: Morning 30% at level 3 + Day 4% at level 10),
# Butterfree 20 (HG-only: Morning 10%/Day 10%) tied with Metapod 20 (Morning
# 20%/Day 0%, a real tie broken in Butterfree's favor since it has an actual
# nonzero Day rate) vs. Pidgeotto 19 (lowest of the bunch). Ledyba outranks
# all of these and takes the very_rare slot; Butterfree, Metapod, and
# Pidgeotto all fall outside the top 5.
ROUTES["MAP_ROUTE2_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRoute2",
    "day": {
        "very_common": [("SPECIES_PIDGEY", 3, 5)],
        "common": [("SPECIES_CATERPIE", 3, 4)],
        "uncommon": [("SPECIES_WEEDLE", 3, 4)],
        "rare": [("SPECIES_KAKUNA", 5, 5)],
        "very_rare": [("SPECIES_LEDYBA", 3, 10)],
    },
    "night": {
        "very_common": [("SPECIES_HOOTHOOT", 3, 5)],
        "common": [("SPECIES_SPINARAK", 3, 4)],
        "uncommon": [("SPECIES_NOCTOWL", 7, 10)],
        "rare": [("SPECIES_WEEDLE", 4, 5)],
        "very_rare": [("SPECIES_KAKUNA", 3, 3)],
    },
}

# Route 3: leads to Mt. Moon. Native HGSS data only fills 4 of 5 tiers for
# both Day and Night (SoulSilver's Ekans covers the 5th native slot; the
# Arbok row Bulbapedia/Serebii list at level 10 is almost certainly a
# transcription artifact — Arbok cannot evolve from Ekans until well past
# that level, so it is dropped rather than treated as a genuine wild
# encounter). The resulting open very_rare tiers get baby-Pokemon Johto
# crossovers that nod at Mt. Moon's Clefairy/Moon Stone lore just ahead.
ROUTES["MAP_ROUTE3_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRoute3",
    "day": {
        "very_common": [("SPECIES_SPEAROW", 5, 8)],
        "common": [("SPECIES_RATTATA", 5, 10)],
        "uncommon": [("SPECIES_EKANS", 8, 8)],
        "rare": [("SPECIES_JIGGLYPUFF", 6, 6)],
        "very_rare": [("SPECIES_IGGLYBUFF", 4, 4)],  # crossover: Johto baby form paired with native Jigglypuff
    },
    "night": {
        "very_common": [("SPECIES_RATTATA", 5, 10)],
        "common": [("SPECIES_ZUBAT", 5, 5)],
        "uncommon": [("SPECIES_EKANS", 8, 8)],
        "rare": [("SPECIES_JIGGLYPUFF", 6, 6)],
        "very_rare": [("SPECIES_CLEFFA", 3, 5)],  # crossover: Johto baby form, nods at Mt. Moon's Clefairy/Moon Stone lore
    },
}

# Route 4: shares Route 3's basic species set (Rattata/Spearow/Jigglypuff/
# Ekans/Zubat) in HGSS. Day leaves one native tier short (see the crossover
# below). Night, once Spearow's real combined rate is counted alongside the
# rest, is fully native across all 6 real candidates (Rattata/Spearow/Zubat/
# Ekans/Arbok/Jigglypuff) — ranked by combined HG+SS Night rate from raw
# Bulbapedia wikitext: Rattata 65 (HG 30% + SS 35%), Zubat 30, Spearow 30
# (HG-only, tie broken behind Zubat), Ekans 20, Jigglypuff 10, Arbok 5
# (lowest, dropped — the sole open native tier's worth of headroom is used up
# by adding Spearow, not by a crossover).
ROUTES["MAP_ROUTE4_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRoute4",
    "day": {
        "very_common": [("SPECIES_SPEAROW", 5, 8)],
        "common": [("SPECIES_RATTATA", 5, 10)],
        "uncommon": [("SPECIES_EKANS", 8, 8)],
        "rare": [("SPECIES_JIGGLYPUFF", 6, 6)],
        "very_rare": [("SPECIES_NATU", 5, 7)],  # crossover: Johto import, foreshadows the mountainous approach to Cerulean Cave
    },
    "night": {
        "very_common": [("SPECIES_RATTATA", 8, 10)],
        "common": [("SPECIES_ZUBAT", 5, 5)],
        "uncommon": [("SPECIES_SPEAROW", 5, 5)],
        "rare": [("SPECIES_EKANS", 8, 8)],
        "very_rare": [("SPECIES_JIGGLYPUFF", 6, 6)],
    },
}

# Route 5: home to the Kanto Day Care. Native Day data, once Morning+Day are
# combined across HG and SS (raw Bulbapedia wikitext), actually lists 4
# species — Pidgey 200 (combined HG+SS Morning+Day), Bellsprout 60, Meowth 40
# (SS-only), Abra 20 — leaving exactly one tier (very_rare) open. Night is
# filled natively once HeartGold's and SoulSilver's tables are combined
# (SoulSilver adds Meowth). The single open Day tier gets a genuine Johto
# grassland crossover (Girafarig, native to Johto's Routes 42-44); the
# Togepi homage pick that used to sit here was displacing real native data
# (Meowth) and has been dropped.
ROUTES["MAP_ROUTE5_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRoute5",
    "day": {
        "very_common": [("SPECIES_PIDGEY", 13, 15)],
        "common": [("SPECIES_BELLSPROUT", 13, 13)],
        "uncommon": [("SPECIES_MEOWTH", 14, 14)],
        "rare": [("SPECIES_ABRA", 12, 14)],
        "very_rare": [("SPECIES_GIRAFARIG", 13, 15)],  # crossover: Johto import, native to Johto's open grassland routes
    },
    "night": {
        "very_common": [("SPECIES_ODDISH", 13, 13)],
        "common": [("SPECIES_MEOWTH", 13, 13)],
        "uncommon": [("SPECIES_BELLSPROUT", 14, 14)],
        "rare": [("SPECIES_GLOOM", 15, 15)],
        "very_rare": [("SPECIES_ABRA", 12, 14)],
    },
}

# Route 6: shares Route 5's basic table shape. Combining HeartGold's
# Magnemite slot with SoulSilver's Meowth slot fills all 5 native tiers for
# both Day and Night, so no open tier remains for a crossover pick here.
ROUTES["MAP_ROUTE6_FRLG"] = {
    "is_johto": False,
    "base_label_prefix": "gKantoRoute6",
    "day": {
        "very_common": [("SPECIES_PIDGEY", 13, 14)],
        "common": [("SPECIES_BELLSPROUT", 13, 13)],
        "uncommon": [("SPECIES_MEOWTH", 14, 14)],
        "rare": [("SPECIES_MAGNEMITE", 15, 15)],
        "very_rare": [("SPECIES_ABRA", 12, 14)],
    },
    "night": {
        "very_common": [("SPECIES_ODDISH", 13, 13)],
        "common": [("SPECIES_MEOWTH", 13, 13)],
        "uncommon": [("SPECIES_BELLSPROUT", 14, 14)],
        "rare": [("SPECIES_MAGNEMITE", 15, 15)],
        "very_rare": [("SPECIES_ABRA", 12, 14)],
    },
}
