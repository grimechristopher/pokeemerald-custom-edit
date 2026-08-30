# Magikarp Jump: Core Data Model

**Branch:** `feature/magikarp-jump`
**Status:** Approved for planning
**Date:** 2026-08-29

## Context

This is sub-project 1 of a larger "Magikarp Jump" effort — a full clone of the mobile
spinoff (tap-timing jump minigame, JP/size training loop, food items, league battles
against other trainers' Magikarp, and unlockable cosmetic forms), built as a standalone
overworld feature (not a Battle Frontier facility). The full feature decomposes into:

1. **Core data model** (this spec)
2. Jump minigame (tap-timing gameplay screen)
3. Training loop (feed items, coach, retire decision)
4. Forms/skins (unlockable cosmetic variants)
5. League battles (opponent generation, rank brackets)
6. Overworld integration (pond location, NPC, entry point)

Unlike the source mobile game — where the Magikarp is an abstract virtual pet untied to
any real owned Pokémon — this clone raises a real party/PC Magikarp. That choice is what
this spec resolves: how JP, size, food history, and cosmetic form persist against a real
Pokémon instance without touching the shared `BoxPokemon` struct every other species also
carries.

Reference: `src/pokemon_size_record.c` (pulled in from `rh-hideout/pokeemerald-expansion`
master) already solves a related, narrower problem — deriving a deterministic "size" from
a real mon's IVs/personality for the FRLG record-Magikarp NPC event. It doesn't solve
persistence of a growing, trainable size/JP over time, but its size-formatting/height
utilities are reusable.

## Goals

- Define the single persistent struct all later Magikarp Jump subsystems read/write.
- Attach training progress to a real, specific Magikarp without risking misattribution to
  a different mon (PID collisions, trades, box moves) and without bloating `BoxPokemon`.
- Handle the mon being evolved, released, or traded away mid-career without leaving
  dangling references or corrupt state.
- Keep v1 minimal: one active trainee at a time, no retirement journal/history log yet.

## Non-goals (deferred)

- Multiple concurrent trainees / a PC-box browsing picker (source game only ever has one
  Magikarp; single-slot matches it and is far simpler).
- A retirement journal/history log of past careers — only a single `bestJP` high-water
  mark is kept for v1. Can be added later as an append-only array without breaking this
  struct's layout (it would be a new field, not a change to existing ones).
- The minigame, training loop, forms, and league subsystems themselves (separate specs).

## Struct and placement

Following this codebase's existing convention for standalone minigame save data
(`struct PokemonJumpRecords pokeJump`, `struct BerryCrush`, `struct BerryPickingResults`,
all plain fields on `struct SaveBlock2`, several gated behind `FREE_*` config flags):

```c
// include/global.h, alongside pokeJump/berryCrush/berryPick
#if FREE_MAGIKARP_JUMP == FALSE
struct MagikarpJump
{
    u32 personality;    // active trainee's PID
    u32 otId;           // trainerId+secretId packed
    u16 species;        // SPECIES_NONE == empty slot; otherwise sanity-checks the match
    u32 currentJP;
    u32 bestJP;         // this life's high-water mark
    u16 size;
    u8  formId;
    u8  foodCounts[9];
    u8  careerCount;
    u8  statusFlags;
    u16 trophyFlags;
    u16 coins;          // account-wide currency; lives here while there's only one slot
};
    /*New*/ struct MagikarpJump magikarpJump;
#endif //FREE_MAGIKARP_JUMP
```

~34 bytes, one instance, no array, appended at the tail of `SaveBlock2` per this
codebase's save-breaking-change convention (see the Sinnoh Battle Frontier spec's save
data strategy) — this is a save-breaking addition and must be flagged as such at merge
time.

`species == SPECIES_NONE` is the sentinel for "no active trainee."

## Identity resolution, not a cached reference

The struct never stores a pointer or party/box index into the trainee. Instead, a single
accessor resolves identity fresh on every call:

```c
struct BoxPokemon *MagikarpJump_GetActiveTrainee(void);
```

This walks the party, then the PC boxes, matching each candidate's
`(personality, otId, species)` against the saved record — the same identity tuple the
game already uses to validate legitimate ownership (shininess/trade-legality checks), so a
false match is not realistically achievable without deliberate save editing. Returns
`NULL` if no match is found (mon evolved, released, or traded away) or if the slot is
empty (`species == SPECIES_NONE`).

Every other subsystem (minigame launch, training menu, league battle) calls this function
first and bails to a "no Magikarp is training right now" message on `NULL`. There is
exactly one failure mode, checked in exactly one place — nothing else needs its own
null-handling logic.

## Lifecycle

- **Designate:** the pond NPC's picker (`ChoosePartyMon`-style flow, matching the existing
  FRLG size-record event's pattern) sets `personality`/`otId`/`species` from the chosen
  mon and resets per-life fields (`currentJP = 0`, `size` = starting value, `foodCounts`
  cleared, `formId` = default, `careerCount++`). `coins`, `bestJP`, and `trophyFlags`
  persist — they're account-wide, not per-life.
- **Auto-retire:** triggered the moment `MagikarpJump_GetActiveTrainee()` fails to resolve
  a match (evolved to Gyarados, released, or traded away). Before clearing, `currentJP`
  and `size` are folded into `bestJP` if they exceed it. Per-life fields
  (`currentJP`, `size`, `foodCounts`, `formId`) then reset to empty state;
  `species` is set back to `SPECIES_NONE`. `coins`/`bestJP`/`trophyFlags` survive
  untouched until the next designation.
- **Empty slot:** checked via `species == SPECIES_NONE` before anything else touches the
  struct — this is the state right after a fresh save and right after any auto-retire.

## Error handling

Centralized entirely in `MagikarpJump_GetActiveTrainee()`. Every caller follows the same
pattern: resolve, bail on `NULL`, otherwise proceed. No subsystem needs to independently
reason about trades, evolution, or box compaction.

## Testing

Testable in isolation, matching this codebase's existing `test/battle_hall.c`-style
convention: fabricate party/box states, designate a trainee, mutate the party (evolve,
release, simulate a trade by removing the mon), and assert that
`MagikarpJump_GetActiveTrainee()` returns `NULL` post-mutation and that `bestJP` correctly
reflects the higher of the old `bestJP` and the life's final `currentJP`. None of this
requires the minigame, training UI, or an actual battle to exist yet.
