# Sinnoh Battle Frontier: Battle Arcade, Battle Castle, Battle Hall

**Branch:** `feature/battle-formats`
**Status:** Approved for planning
**Date:** 2026-08-28

## Context

This is the first of three queued sub-projects under a larger "new battle formats" effort
(order: **Sinnoh Battle Frontier facilities** → Battle Chateau → Battle Agency). Rotation
Battles, Triple Battles, and Battle Royal were identified as much larger architectural
lifts (they touch battler-count assumptions throughout the battle engine) and are
deliberately not part of this queue yet.

Pokeemerald-expansion already implements Hoenn's Battle Frontier in full (Battle Tower,
Dome, Palace, Arena, Factory, Pike, Pyramid) plus the pre-Frontier Battle Tent, all built
on a shared `frontier_util.c`/`struct BattleFrontier` foundation. Sinnoh's Battle Frontier
(introduced in Platinum) reuses the Tower and Factory formats but adds three facilities
that don't exist here yet:

- **Battle Arcade** — a roulette wheel rolls a random effect before each of 7 battles
- **Battle Castle** — battles are wagered/scored via a Castle Points (CP) economy
- **Battle Hall** — single-Pokémon battles against a type-locked, rank-scaled opponent pool

## Goals

- Implement the distinctive mechanics of all three facilities well enough to play a full
  round/streak from the debug menu.
- Follow the existing Frontier facility conventions (`battle_dome.c`, `battle_pike.c`,
  `frontier_util.c`) rather than inventing a parallel pattern.
- Reuse the existing shared Battle Points economy, Frontier level-50/Open level modes, and
  banned-species lists already used by Tower/Factory/etc. — no new currency systems except
  Castle's CP, which is a real, self-contained in-run currency per the source games.

## Non-goals (deferred to later phases)

- Overworld facility maps, NPCs, Frontier Brain trainers (Dahlia, Darach, Argenta), and
  Frontier Pass integration. This spec is debug-menu-launchable only.
- Any of Battle Chateau or Battle Agency (queued next).
- Rotation/Triple/Royal battle formats.

## Save data strategy

`struct BattleFrontier` (`include/global.h`) is a byte-offset-annotated struct serialized
directly into `SaveBlock2`. Per this project's stated saveblock philosophy, this spec's
new persistent fields (win streaks, record streaks, prints, and per-type Hall rank
progress) **are a save-breaking change** and must be called out and batched at merge time
like any other save-breaking feature — this codebase has no automatic save-migration
framework, so the fields must be appended at the tail of `struct BattleFrontier` to
minimize disruption to existing offsets, and the PR must be flagged as save-breaking.

In-progress, non-milestone state (current CP total and shop purchases mid-challenge, the
Arcade's per-round performance score, which panel was last rolled) stays in transient
EWRAM structs, matching how other facilities keep challenge-in-progress scratch data
separate from the persisted streak/record fields.

## Battle type flags

Only **Battle Arcade** needs a new `BATTLE_TYPE_ARCADE` flag, claiming one of the four
currently-unused bits in `gBattleTypeFlags` (`BATTLE_TYPE_14`, `_28`, `_29`, or `_30` in
`include/constants/battle.h`) — its panels can change starting weather, field status, and
mon state before turn 1, which the battle engine needs to know about. Battle Castle and
Battle Hall run ordinary `BATTLE_TYPE_TRAINER` battles; everything distinctive about them
(the CP shop, the type/rank opponent generator) lives in the facility-controller layer
above the battle, the same way Dome and Pike already wrap ordinary battles. This leaves
three bits still free for future work.

## Battle Arcade

**Files:** `src/battle_arcade.c`, `include/battle_arcade.h`, new save fields on
`struct BattleFrontier` (`arcadeWinStreaks`/`arcadeRecordWinStreaks`/`arcadePrize`,
following the existing per-facility naming convention).

**Structure:** 3 Pokémon per team, levels reduced to 50, held items removed before
battling, 7 consecutive battles per round (matches existing Frontier structure/level-mode
conventions already used by Tower/Factory).

**Roulette panels** (`sArcadePanels[]`), each tagged with a target
(`ARCADE_TARGET_PLAYER` / `ARCADE_TARGET_OPPONENT` / `ARCADE_TARGET_BOTH`) and an effect:

| Effect | Notes |
|---|---|
| Cut HP by 20% | |
| Inflict Poison / Paralysis / Burn / Sleep / Freeze | |
| Grant a berry or held item | Lasts the rest of the round for the player; a single battle for the opponent |
| Raise level by 3 | Lasts one battle for the player; the opponent's raised level is what the player actually fights |
| Battle in Sun / Rain / Sandstorm / Hail | Sets `gBattleWeather` at battle init |
| Fog | |
| Trick Room (5 turns) | Sets `gFieldStatuses \| STATUS_FIELD_TRICK_ROOM` with a 5-turn timer at battle init |
| Swap teams with the opponent | |
| Speed up / down the roulette | Affects only the *next* roll's spin duration, cosmetic |
| Randomize roulette order | Cosmetic, no battle effect |
| Grant 1 or 3 BP | Added directly to `gSaveBlock2Ptr->frontier.battlePoints` |
| Skip this battle | Advances the round without a fight |
| No event | Battle proceeds normally |

**Selection weighting:** matches the source games' behavior — a hidden per-round
performance score (EWRAM, resets at the start of every round of 7) shifts the odds toward
neutral/field panels as the player performs better, and toward player-favorable panels as
they perform worse. Reuses the weighted-selection approach already present in
`battle_pike.c`'s room-hazard selection rather than inventing a new one.

**Battle engine touch point:** Arcade panel effects that affect party data (status, HP,
level, items, team swap) are applied to `gPlayerParty`/`gEnemyParty` before
`BattleSetup_StartTrainerBattle` is called — no engine change needed there. Weather and
Trick Room are the only two effects that need a new, narrow hook at battle
initialization (`battle_main.c`): when `BATTLE_TYPE_ARCADE` is set, read a pending
weather/field-status override (set by `battle_arcade.c` right before the battle starts)
instead of always resetting `gBattleWeather` to 0 and leaving Trick Room off.

## Battle Castle

**Files:** `src/battle_castle.c`, `include/battle_castle.h`, new save fields
(`castleWinStreaks`/`castleRecordWinStreaks`/`castlePrize`).

**Structure:** 3 Pokémon per team, levels reduced to 50, held items removed. 21
consecutive wins to face Darach for the Silver Print, 49 for the Gold Print (matches
Tower/Dome's existing streak-gated Frontier Brain convention).

**Castle Points (CP):** Player starts a challenge with 10 CP (in-progress CP total is
EWRAM, not saved, matching the non-goal of full challenge persistence in this phase).

Earned per battle via `CalculateCastlePoints()`, capped at 50 CP/battle:

- ×3 per Pokémon that didn't faint
- ×3 per Pokémon at full HP (else ×2 if ≥50% HP, ×1 if <50%)
- ×1 per Pokémon with no status ailment
- PP-efficiency bonus: 8 CP if ≤5 total PP used this battle, 6 for 6–10, 4 for 11–15, 0 above that
- ×7 bonus per 5 levels the player chose to raise the opponent by (see shop, below)

**Pre-battle shop**, CP-costed, presented before each battle:

- Scout the opponent's species (1 CP) or full moveset (5 CP, unlocked at Rank 2)
- Raise or lower the opponent's level by 5 per step (1–15 CP depending on tier)
- Heal HP (10 CP), PP (8 CP), or both (12 CP); status ailments are otherwise always
  cleared between battles for free, matching the source games
- Buy berries/held items (2–20 CP depending on rank)

Losing (or withdrawing) resets the streak and CP back to the base 10, exactly as in the
source games — no partial-credit carryover.

**Battle:** ordinary `BATTLE_TYPE_TRAINER` battle; no new battle-engine mechanics needed.

## Battle Hall

**Files:** `src/battle_hall.c`, `include/battle_hall.h`, new save field
(`hallTypeRanks[NUMBER_OF_MON_TYPES]`, persisting rank 1–10 progress per type).

**Structure:** Single Battles use one Pokémon, level 30+; Double Battles require two of
the same species. 10 battles per round; BP awarded at round end, with bonus BP at
streak milestones (10/30/50+), matching the source games.

**Opponent generation** (`GetHallOpponentSpecies(type, rank)`): rather than hand-porting
Gen 4's fixed per-species tier list — which won't line up with this hack's
expanded/custom Pokédex — species are bucketed programmatically by base stat total (BST)
within the chosen type, using the source games' BST-to-rank-availability brackets (e.g.
BST < 340 available from Rank 1, BST 340–439 from Rank 3, BST 440–499 from Rank 6, and so
on up to Rank 10), filtered through whatever banned-species list the existing Frontier
facilities already use to exclude legendaries/mythicals. This makes the facility
automatically correct for any species roster the hack maintainer configures, rather than
requiring a hand-curated list to be kept in sync.

Rank scaling (opponent level, and how many types must be advanced before harder brackets
unlock) follows the formula referenced from the source games: a function of the player's
Pokémon's level, the chosen type's current rank, and how many other types have progressed
past Rank 1.

**Battle:** ordinary `BATTLE_TYPE_TRAINER` battle (single or same-species double); no new
battle-engine mechanics needed.

## Debug menu integration

Extends the existing `DEBUG_BATTLE` submenu in `src/debug.c` (which already has
WILD/WILD_DOUBLE/SINGLE/DOUBLE/MULTI entries) with three new entries:

- **Start Arcade Round** — choose a specific panel to force (for testing each effect) or
  roll randomly, then launches a 3v3 level-50 battle using the existing debug trainer
  party infrastructure (`sDebugTrainers`).
- **Start Castle Battle** — grants the debug session 50 CP, presents the shop menu, then
  launches the battle; CP earned is displayed post-battle via `CalculateCastlePoints()`.
- **Start Hall Battle** — choose a type, launches a battle against a generated opponent
  at the current (or manually set) rank for that type.

No map, NPC, or Frontier Pass flag is required to reach any of these.

## Testing

- `CalculateCastlePoints()` and `GetHallOpponentSpecies()` are pure-ish functions and get
  direct unit-style tests (no full battle needed) covering the formula's boundary
  conditions (e.g. exactly 50 CP cap, each PP-tier threshold, each BST bracket edge).
- Battle Arcade's pre-battle panel effects (starting weather, status, Trick Room) are
  well-suited to the existing `SINGLE_BATTLE_TEST` GIVEN/WHEN/SCENE/THEN DSL — GIVEN a
  forced panel, THEN assert the expected starting condition.
- Manual/interactive verification of the shop flow, roulette UI, and full round/streak
  play happens via the new debug menu entries.

## Risks / open items

- **Save-breaking change.** Flagged above; needs explicit sign-off at merge time per the
  project's version-batching policy, since existing players' Frontier save data will not
  automatically migrate.
- Battle Arcade's weather/Trick-Room-at-init hook is the one genuine battle-engine
  change in this spec; it should be scoped as narrowly as possible (gated entirely behind
  `BATTLE_TYPE_ARCADE`) to avoid touching behavior for every other battle type.
- Exact wording/ordering of Castle's shop menu and Hall's type-select menu is left to
  implementation-time UI conventions already used elsewhere in `frontier_util.c`.
