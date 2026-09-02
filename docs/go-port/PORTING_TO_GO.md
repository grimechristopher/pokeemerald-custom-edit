# Porting pokeemerald-expansion to Go: Feasibility & Strategy

Status: **analysis only — no code ported yet.** This document exists to answer one
question before any implementation work starts: *what would a "one-to-one" port of
this codebase to Go actually involve, and is it realistic?*

## TL;DR

- **A literal one-to-one port — same binary target, GBA cartridge ROM, produced from
  Go instead of C — is not realistic.** TinyGo has an experimental ARM7TDMI/GBA
  backend (see `tinygo.org/x/tinygba`), but it's aimed at small homebrew demos, not
  a ~1M-line game with a GC-hostile, interrupt-driven, 32 KB-IWRAM/256 KB-EWRAM
  memory budget. Go's runtime (goroutine scheduler, garbage collector) doesn't fit
  the platform this project targets.
- **A one-to-one port of *behavior*, targeting a native/desktop Go engine instead of
  real GBA hardware, is realistic** and is what "port to Go" should mean here. This
  is a from-scratch reimplementation of the same game logic and data, running under
  a Go game engine (Ebitengine is the natural choice) instead of on GBA silicon.
  "One-to-one" then means: same data tables, same battle/overworld/menu logic, same
  RNG-driven outcomes where it matters, same feature set — not the same compiled
  bytes.
- This is a **large, multi-year-scale undertaking** if done exhaustively (the
  battle engine and its move effects alone are ~18K lines of C interpreting ~90K+
  lines of "battle script" data). It is realistically only worth doing incrementally,
  subsystem by subsystem, with its own test suite proving parity as you go.

## Why "same ROM, written in Go" doesn't work

This project isn't just game logic — it's tightly coupled to GBA hardware:

- **463 distinct hardware register macros** (`REG_*`) are referenced directly across
  `src/`/`include/` — DMA, timers, PPU (background/OAM/palette control), keypad,
  serial ports. All memory-mapped I/O, not something a hosted Go runtime has access
  to or would want to touch directly.
- **BIOS/SWI calls and naked functions**: interrupt handlers, `VBlankIntrWait`, and
  hand-tuned ARM/Thumb routines (`libagbsyscall`, several `.s` files under
  `src/` and `asm/`) assume the ARM7TDMI SWI calling convention and cycle-level
  timing (audio mixing especially — `src/m4a_1.s` is raw hand-written assembly).
- **Save data** is a byte-exact struct layout serialized to flash/SRAM
  (`src/save.c`, `SaveBlock1`/`SaveBlock2`, sector-based wear-leveling) — a format
  designed around GBA cart flash chips, not something a Go program needs to
  replicate unless save-file compatibility with real GBA saves/emulators is a goal.
- **Link cable / multiplayer** hardware protocol has no desktop equivalent; it would
  need to become a network protocol, not a port.
- The **build pipeline** itself assumes a GBA target: linker script
  (`ld_script_modern.ld`), `gbafix` (ROM header fixup), `compresSmol` (LZ77/Huffman
  compression to fit assets in a 32 MB cart), custom graphics tooling (`gbagfx`,
  `porytiles`) producing GBA tile/palette formats from ~18K graphics assets.

None of this has a meaningful Go equivalent on GBA hardware, and TinyGo's GBA
target — real but explicitly experimental — doesn't have the maturity or the
memory headroom to absorb a project this large (see `tinygo.org/x/tinygba`,
`danacr/tiny-gba`, `xen0bit/gbablog` — all small tech-demo scale).

**Conclusion: target a desktop/native Go reimplementation, not a GBA ROM.**

## What "one-to-one" means once the target changes

Once the target is "a Go program that plays like this game" rather than "a GBA ROM
compiled from Go," one-to-one porting becomes a question of which layers get ported
literally (data, algorithms, control flow) versus which layers get replaced with a
Go-native equivalent (rendering, audio mixing, input, save format):

| Layer | Port strategy |
|---|---|
| Data tables (species, moves, items, abilities, learnsets, trainers, encounters, evolution, TM/HM, maps, text) | **Literal port.** These are already data, mostly in `.h`/`.json`. Convert straight to Go structs/slices or embed as JSON/binary via `go:embed`. No logic to reinterpret. |
| Battle engine control flow (`battle_main.c`, `battle_util*.c`, `battle_script_commands.c`) | **Literal port**, function-by-function. This is portable C: state machines and math, no hardware calls. Straightforward (if enormous) 1:1 translation. |
| Battle "scripts" (`data/battle_scripts_*.s`, `battle_anim_scripts.s`) | These are a custom bytecode compiled from asm macros, interpreted by `battle_script_commands.c`. Two options: (a) port the bytecode format and interpreter literally, or (b) — recommended — re-express each script as a Go data structure (slice of opcodes/args) generated once from the existing `.s`, dropping the assembler step entirely. Same behavior, no custom toolchain needed. |
| Overworld event scripts (`data/event_scripts.s`, `src/scrcmd.c`, poryscript-compiled `.inc`) | Same shape as battle scripts: a bytecode VM (`scrcmd.c`) over compiled script data. Port the VM logic 1:1; keep script *authoring* in poryscript if desired, or migrate to a Go-native scripting representation later. |
| RNG (`src/random.c`) | **Literal port**, and this one matters: if any output should match the original bit-for-bit (tests, shared seeds, community expectations), the exact LCG algorithm and call order must be preserved, not just "a random number." |
| Pokémon data structure & stat/EV/IV calc (`src/pokemon.c`, `include/pokemon.h`) | Literal port of the math; the substruct/encryption scheme that exists purely to fit GBA save constraints (`SaveBlock1`/box Pokémon substruct shuffling/encryption) is a candidate to drop or simplify since Go has no equivalent flash-wear constraint — a deliberate *deviation* from one-to-one, worth flagging explicitly rather than cargo-culting. |
| Graphics rendering (PPU: backgrounds, sprites/OAM, palettes, blending, mode 0-2) | **Not portable, must be reimplemented** as a small "virtual PPU" layer on top of Ebitengine (or similar): sprite/tile compositing, palette swaps, priority, window/blend effects reimplemented against Ebitengine's draw calls instead of GBA VRAM writes. This is the single largest non-literal chunk of work. |
| Audio (`src/m4a*.c/.s`, `sound/`) | **Not portable.** `m4a` is a cycle-tuned GBA sound mixer in hand-written assembly. Reimplement music/SFX playback using a Go audio library, driven by the same song/sequence data (MIDI-derived) after conversion, rather than porting the mixer itself. |
| Save format | Reimplement as a straightforward Go serialization (JSON/gob/protobuf) of the same logical fields; drop the sector-rotation/checksum machinery that exists only for GBA flash wear-leveling, unless GBA save-file interop is an explicit goal. |
| Build tooling (`tools/`: `gbagfx`, `mapjson`, `jsonproc`, `mid2agb`, `wild_encounters` scripts, etc.) | Mostly **obsolete** in a Go port — they exist to convert human-editable assets into GBA-native binary formats under ROM-size pressure. A Go port has no cartridge-size constraint, so assets can stay as PNG/JSON/OGG loaded at runtime; only the *asset conversion intent* (e.g. "these frames make this animation") needs to be ported, not the compressors. |
| Test framework (`test/battle/*`, GIVEN/WHEN/SCENE/THEN DSL, `mgba-rom-test-hydra`) | The DSL concept ports well (it's decoupled from GBA already at the *authoring* level), but the current harness runs the compiled ROM in mGBA and asserts on emulated framebuffer/text output. A Go port needs an equivalent harness that drives the Go engine directly and asserts on its (also virtual) UI state — a new test runner, same test-writing philosophy. |

## Scope, by the numbers (this checkout)

Gathered directly from the tree to size the effort:

- `src/*.c`: 349 files, ~506K lines
- `src/data/*.h` (data tables included by the above): 153 files, ~514K lines
- `include/*.h`: 412 files, ~62K lines
- `data/*.s` (battle/overworld/field scripts + misc tables): 49.8K lines
- `test/*.c`: 937 files, ~99K lines
- Graphics assets: ~18.2K files; JSON data files: 520; map directories: 519
- `battle_script_commands.c` alone (the battle bytecode interpreter): 18K lines

Order of magnitude: **~1.1M lines of C/data plus ~18K graphics assets and ~500
maps.** Even assuming Go ends up more concise than the C in places (less
boilerplate around GBA-specific packing/encryption once that's dropped), this is
easily a multi-year effort for a small team if pursued as genuinely one-to-one
(every move effect, every overworld interaction, every menu). It is *not* a
weekend or single-agent-session scope, and shouldn't be planned as one.

## Recommended approach if this is pursued

1. **Pick the target stack first** and prove the hardest non-literal piece early:
   a minimal Go/Ebitengine "virtual PPU" that can render one real background +
   sprites from converted assets. This de-risks the biggest unknown before
   investing in data porting.
2. **Port data before logic.** Species/move/item/ability tables are pure data and
   can be mechanically converted (script-assisted, from the existing `.h`/JSON)
   with high confidence and immediate testability.
3. **Port the two interpreters (battle script VM, overworld script VM) before the
   thousands of individual scripts.** Once the VM is faithful, script data
   conversion becomes mechanical and incremental — port scripts/move-effects in
   test-covered batches (e.g. by move effect, mirroring this repo's own
   `test/battle/move_effect/` organization) rather than all at once.
4. **Build the new test harness before porting battle logic**, reusing the
   GIVEN/WHEN/SCENE/THEN vocabulary against the Go engine. The existing
   `test/battle/*` suite becomes the parity oracle: same test *names and intent*,
   re-expressed against the Go runner, is how "one-to-one" gets verified rather
   than asserted.
5. **Treat GBA-only concerns (save sector wear-leveling, substruct
   shuffle-encryption, link cable protocol, ROM-size-driven compression) as
   explicitly out of scope for literal porting** — reimplement their *intent*
   (persist a save; sync two players; fit in memory) with normal Go idioms, and
   record each such deviation so "one-to-one" claims stay honest about what
   changed and why.
6. **Expect the RNG and any cross-generation formulas (damage calc, catch rate,
   experience) to need bit-for-bit fidelity** if any comparison against the
   original ROM/community expectations matters — these are the parts most worth
   being pedantic about porting literally, function-for-function.

## Open questions for whoever picks this up

- Is GBA-save-file interoperability (importing/exporting real `.sav` files) a
  requirement, or is the save format free to become Go-native?
- Is bit-exact RNG/damage-roll parity with the real ROM required (e.g. for
  side-by-side testing against mGBA), or is "plays the same" sufficient?
- Rendering target: Ebitengine was assumed above as the natural 2D Go engine
  (cross-platform, active, simple API) — worth confirming before building the
  virtual-PPU layer, since that choice shapes a lot of downstream code.
- Full expansion feature set (this fork, `pokeemerald-expansion`) or upstream
  `pokeemerald` only? The expansion adds substantial config-gated surface area
  (`include/config/*.h`) on top of an already large base.
