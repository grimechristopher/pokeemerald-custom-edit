# About `pokeemerald-expansion`

![Gif that shows debugging functionality that is unique to pokeemerald-expansion such as rerolling Trainer ID, Cheat Start, PC from Debug Menu, Debug PC Fill, Pokémon Sprite Visualizer, Debug Warp to Map, and Battle Debug Menu](https://github.com/user-attachments/assets/cf9dfbee-4c6b-4bca-8e0a-07f116ef891c) ![Gif that shows overworld functionality that is unique to pokeemerald-expansion such as indoor running, BW2 style map popups, overworld followers, DNA Splicers, Gen 1 style fishing, OW Item descriptions, Quick Run from Battle, Use Last Ball, Wild Double Battles, and Catch from EXP](https://github.com/user-attachments/assets/383af243-0904-4d41-bced-721492fbc48e) ![Gif that shows off a number of modern Pokémon battle mechanics happening in the pokeemerald-expansion engine: 2 vs 1 battles, modern Pokémon, items, moves, abilities, fully customizable opponents and partners, Trainer Slides, and generational gimmicks](https://github.com/user-attachments/assets/50c576bc-415e-4d66-a38f-ad712f3316be)

<!-- If you want to re-record or change these gifs, here are some notes that I used: https://files.catbox.moe/05001g.md -->

**`pokeemerald-expansion`** is a GBA ROM hack base that equips developers with a comprehensive toolkit for creating Pokémon ROM hacks. **`pokeemerald-expansion`** is built on top of [pret's `pokeemerald`](https://github.com/pret/pokeemerald) decompilation project. **It is not a playable Pokémon game on its own.**

# ⚠️ This Fork's Save-Format Deviations

This fork ([`grimechristopher/pokeemerald`](https://github.com/grimechristopher/pokeemerald), branch `expanded/base`) targets an emulator-only, single-player build and trades away a few things upstream `pokeemerald-expansion` keeps for hardware/vanilla-save compatibility:

- **No `BoxPokemon` encryption or substruct shuffling.** Stock Emerald XORs the "secure" region of each Pokémon against `personality ^ otId` and permutes the four substructs into one of 24 orders keyed off `personality % 24` — obfuscation meant to make save-editing on real hardware/flash carts harder. With no real-hardware or vanilla-save compatibility to protect, that cost bought nothing here, so it's gone: `BoxPokemon.secure` is now a fixed, named struct (`substruct0`..`substruct3`) at constant offsets, stored unencrypted. Substructs also no longer get forced to a common padded size — though as of this writing all four still happen to be 12 bytes anyway, so `sizeof(struct BoxPokemon)` is unchanged at 80 bytes; the win is that a future resize won't drag the others up with it.
- **No backup save slot.** Stock Emerald keeps two full copies of the save and alternates writes between them so a corrupted write can fall back to the other copy. This build keeps one, reclaiming ~248 KB of flash.
- **72 PC boxes** (up from 14), backed by an expanded 17-sector `SaveBlock1`.

None of this is compatible with real Game Boy Advance hardware, save-editing tools built for vanilla saves, or vanilla `pokeemerald-expansion` saves — it's tuned specifically for this project's emulator-only target. If you need any of the above, use upstream `pokeemerald-expansion` instead.

# 🌗 This Fork's Time & Atmosphere Settings

Unlike the save-format changes above, these are just non-default values for config toggles that already exist upstream — nothing save-breaking, and easy to flip back in `include/config/overworld.h` if you don't want them.

- **Non-real-time clock (`OW_USE_FAKE_RTC`).** The in-game clock runs on a simulated `SiiRtcInfo` kept in the save file (`gSaveBlock3Ptr->fakeRTC`) instead of tracking real hardware/host time. At `OW_ALTERED_TIME_RATIO`'s default (`GEN_9`), it advances 20 in-game seconds per real second — a full day/night cycle in about **72 real minutes**, the same pacing Scarlet/Violet ships with.
- **Time-of-day wild encounters (`OW_TIME_OF_DAY_ENCOUNTERS`).** Every existing encounter table was duplicated across all four periods (Morning/Day/Evening/Night) via the expansion's own migration script, so nothing goes quiet at night — they're just identical for now. Differentiating what actually appears when is a design decision for later, not something baked in here.
- **DNS window lighting on vanilla Hoenn maps.** The Dynamic Overworld Palette engine (`OW_ENABLE_DNS`) ships on by default upstream, but no maps used it out of the box. This fork restores upstream's own prior light-blending work (`.pla` files, alternate night palettes, lamp/candle/sign sprites) across Dewford, Ever Grande, Lavaridge, Lilycove, Mauville, Mossdeep, Pacifidlog, Petalburg, Route 102, Rustboro, Slateport, Sootopolis, and Verdanturf — it had been added upstream and then explicitly reverted; this fork un-reverts it.

# 🔡 Decapitalized Text

Vanilla Gen 3 renders proper nouns in ALL CAPS ("POKéMON", "TRAINER", a character's name) — a holdover from the original games' technical limitations. This fork uses `TEXT_CAPITALIZE`'s `[bracket]` mechanism (built by upstream's `upcoming-decap` branch, still unmerged there as of this writing) to render them in normal case instead: "Pokémon", "Trainer", "Wally". `TEXT_CAPITALIZE` stays at its default (`FALSE`) to get this; setting it `TRUE` reverts to the vanilla all-caps look.

Since upstream's own decap branch is only partially done (map dialogue converted A–Z is nowhere near finished there), this fork instead extracted the editorial decisions from [Prof. Harpe's ~99%-complete decap of pokeemerald-expansion](https://github.com/prof-harpe/pokeemerald-expansion) and replayed them against this tree as `[bracket]` tags — roughly 5,500 spans across 375 files, covering the bulk of battle text, menus, contest text, the HGSS Dex, and map dialogue across the full alphabet. Not literally everything: content this fork added since (Game Corner minigames, Magikarp Jump, etc.) predates none of that work and isn't covered, and a conservative extraction process deliberately skipped any change that looked like more than a pure recase rather than risk corrupting it.

# 🚧 This Fork's Custom Additions

Beyond the deviations above, this fork has its own in-progress feature work layered on top of `pokeemerald-expansion`:

- **8-directional overworld movement (`OW_DIAGONAL_MOVEMENT`).** The player and NPCs can move diagonally, matching Gen 6+, instead of being locked to the four cardinal directions. Covers input handling, no-corner-cutting collision, Bikes/Surf/Dive, ledges and arrow warps resolving like stairs, wandering NPCs, and followers. On by default; see `include/config/overworld.h`. Design/implementation history in `docs/superpowers/specs/` and `docs/superpowers/plans/` (`2026-09-02-diagonal-movement-*`).
- **Ranger-style Capture Styler (`B_RANGER_CAPTURE`).** A new item, `ITEM_CAPTURE_STYLER`, that swaps the usual RNG-based catch odds for a rhythm minigame (`src/ranger_capture.c`) when thrown in a wild battle — hit the matching icon (D-pad/A/B) on a single scrolling reel, and dodge Attack icons, in time with the target to fill a loop-progress meter and close a capture ring around it before running out of misses. Difficulty (loops needed, note speed, miss allowance) scales off the target's catch rate and status. On by default.
- **Magikarp Jump pattern forms.** All 31 cosmetic Magikarp/Gyarados color patterns from *Pokémon: Magikarp Jump* exist as real species (62 total) with working evolution and Mega Evolution, accessible today in debug mode. They currently render with placeholder (standard Magikarp/Gyarados) graphics pending custom sprites — see `MAGIKARP_JUMP_IMPLEMENTATION_SUMMARY.md` and `MAGIKARP_JUMP_GRAPHICS_GUIDE.md` for status and next steps.

# [Features](FEATURES.md)

**`pokeemerald-expansion`** offers hundreds of features from various [core series Pokémon games](https://bulbapedia.bulbagarden.net/wiki/Core_series), along with popular quality-of-life enhancements designed to streamline development and improve the player experience. A full list of those features can be found in [`FEATURES.md`](FEATURES.md).

# [Credits](CREDITS.md)

 [![](https://img.shields.io/github/all-contributors/rh-hideout/pokeemerald-expansion/upcoming)](CREDITS.md)

If you use **`pokeemerald-expansion`**, please credit **RHH (Rom Hacking Hideout)**. Optionally, include the version number for clarity.

```
Based off RHH's pokeemerald-expansion 1.16.3 https://github.com/rh-hideout/pokeemerald-expansion/
```

Please consider [crediting all contributors](CREDITS.md) involved in the project!

# Choosing `pokeemerald` or **`pokeemerald-expansion`**
 
- **`pokeemerald-expansion`** supports multiplayer functionality with other games built on **`pokeemerald-expansion`**. It is not compatible with official Pokémon games.
- If compatibility with official games is important, use [`pokeemerald`](https://github.com/pret/pokeemerald). Otherwise, we recommend using **`pokeemerald-expansion`**.
- **`pokeemerald-expansion`** incorporates regular updates from `pokeemerald`, including bug fixes and documentation improvements.

# [Getting Started](INSTALL.md)

❗❗ **Important**: Do not use GitHub's "Download Zip" option as it will not include commit history. This is necessary if you want to update or merge other feature branches.

If you're new to git and GitHub, [Team Aqua's Asset Repo](https://github.com/Pawkkie/Team-Aquas-Asset-Repo/) has a [guide to forking and cloning the repository](https://github.com/Pawkkie/Team-Aquas-Asset-Repo/wiki/The-Basics-of-GitHub). Then you can follow one of the following guides:

## 📥 [Installing **`pokeemerald-expansion`**](INSTALL.md)
## 🏗️ [Building **`pokeemerald-expansion`**](INSTALL.md#Building-pokeemerald-expansion)
## 🚚 [Migrating from **`pokeemerald`**](INSTALL.md#Migrating-from-pokeemerald)
## 🚀 [Updating **`pokeemerald-expansion`**](INSTALL.md#Updating-pokeemerald-expansion)

# [Documentation](https://rh-hideout.github.io/pokeemerald-expansion/)

For detailed documentation, visit the [pokeemerald-expansion documentation page](https://rh-hideout.github.io/pokeemerald-expansion/).

# [Contributions](CONTRIBUTING.md)
If you are looking to [report a bug](CONTRIBUTING.md#Bug-Report), [open a pull request](CONTRIBUTING.md#Pull-Requests), or [request a feature](CONTRIBUTING.md#Feature-Request), our [`CONTRIBUTING.md`](CONTRIBUTING.md) has guides for each.

# [Community](https://discord.gg/6CzjAG6GZk)

[![](https://dcbadge.limes.pink/api/server/6CzjAG6GZk)](https://discord.gg/6CzjAG6GZk)

Our community uses the [ROM Hacking Hideout (RHH) Discord server](https://discord.gg/6CzjAG6GZk) to communicate and organize. Most of our discussions take place there, and we welcome anybody to join us!
