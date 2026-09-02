# Porting pokeemerald-expansion to Go: Feasibility & Strategy

Status: **analysis only — no code ported yet.**

**Target, confirmed:** Go executables (Windows/Linux/macOS, and ideally mobile) —
**not** a GBA ROM. No cartridge, no `gbafix`, no ROM-size budget. This rules out
TinyGo's experimental GBA backend as the target entirely; it's not needed here.

**Approach, confirmed:** two stages.
1. **Stage 1 — one-to-one port.** Mechanically faithful translation of the C game
   logic to Go, running as a native executable. Prioritizes behavioral fidelity
   over idiomatic Go. Ugly is fine here.
2. **Stage 2 — cleanup pass.** Refactor the Stage 1 output into proper, idiomatic
   Go: real packages, real types, real error handling, C-isms removed — *without*
   changing behavior, using Stage 1 as the parity baseline.

## Precedent: pokeemerald-multiplatform

[gradenGnostic/pokeemerald-multiplatform](https://github.com/gradenGnostic/pokeemerald-multiplatform)
already did the hard part of this problem once, in C, and it's worth using as the
architectural reference rather than starting from zero:

- It runs the **decompiled game logic directly, essentially unmodified** — it does
  not rewrite the battle/overworld engine. Only the hardware-facing layer changes.
- It replaces GBA hardware (PPU, APU, keypad) with an **SDL2-backed layer**:
  SDL2 rendering with aspect-ratio-preserving scaling, and a "repaired" portable
  build of the MP2K/M4A music player/mixer running through SDL2's float audio
  output (42060 Hz).
- It builds and runs on **Windows, Linux, and Android** from that one abstraction
  layer, using `Makefile_pc` alongside the existing GBA `Makefile`, plus a Gradle/
  NDK path for Android.

The takeaway: **the split between "portable game logic" and "hardware layer" is
already proven to work for this exact codebase.** The Go port should copy that
split, not invent a new one — replace the same seam (REG_* register access, the
m4a mixer, SDL2/keypad input) with a Go equivalent, and treat everything on the
logic side of that seam as a straightforward translation target.

## Stage 1: one-to-one port

### The seam: what gets replaced vs. what gets translated

Same division the precedent project draws, translated to "replaced with a Go
library" instead of "replaced with SDL2 C code":

| GBA-side (C) | Stage 1 Go replacement |
|---|---|
| `REG_*` memory-mapped I/O (463 distinct registers referenced), PPU modes 0-2, OAM/sprites, BG/palette/window/blend | A "virtual PPU" package driving a Go rendering backend (see below) — reimplemented once, informed by both the original PPU semantics *and* pokeemerald-multiplatform's already-working SDL2 renderer as a second reference implementation to check against. |
| `src/m4a*.c/.s` (hand-tuned ARM mixer), `sound/` assets | A Go audio package playing the same sequence/sample data, informed directly by pokeemerald-multiplatform's "repaired portable M4A player" — that's a from-scratch mixer rewrite they already validated once; use its output/behavior as the spec instead of re-deriving GBA audio timing from the original assembly. |
| Keypad registers, link cable hardware | Go backend's input polling; link cable becomes a network protocol (out of scope for Stage 1, stub it) |
| BIOS/SWI calls, interrupt handlers, naked functions | Ordinary Go function calls / a game-loop ticker; no interrupt model needed on a general-purpose OS |
| Save to flash/SRAM sectors, wear-leveling | Go file I/O; keep the *logical* save fields, translate the sector/checksum machinery to a plain file write |
| Build pipeline (`gbafix`, `compresSmol`, `ld_script_modern.ld`) | Not needed at all — no cartridge, no ROM-size pressure. Assets load from disk/`go:embed` uncompressed or with standard Go compression, not GBA tile/LZ77 formats. |

Everything else — battle engine, overworld engine, menus, Pokémon data model, RNG,
script interpreters, data tables — sits on the "logic" side of that seam and is
translation, not redesign.

### Rendering/audio backend choice

Two real options; recommend the first:

- **Ebitengine (recommended).** Pure Go, no cgo, single static binary — closest
  fit to "just Go executables." Cross-compiles to Windows/Linux/macOS from one
  codebase, and already has first-class Android/iOS (via `gomobile`) and
  browser/WASM support, which covers the same platform list pokeemerald-
  multiplatform targets (plus more) without needing a parallel Gradle/NDK build
  path. Downside: pokeemerald-multiplatform's SDL2 code (aspect-ratio math,
  audio-timing fixes) can only be used as a *reference*, not reused directly.
- **go-sdl2 (cgo bindings to real SDL2).** Closer to the precedent project —
  its rendering/scaling math and audio backend choices translate almost
  line-for-line. Trade-off: cgo build (slower cross-compilation, needs SDL2
  present at build/runtime on each target, less "just a Go binary").

Either works; Ebitengine is the better fit for the stated goal ("just go
executables," multiplatform including mobile) and is what the rest of this doc
assumes, but this is worth locking in explicitly before writing the PPU package,
since it shapes everything downstream.

### Using automated C→Go transpilation to accelerate translation

Given the scope (below), doing Stage 1 by hand file-by-file is the main cost driver.
Worth evaluating first: **automated C→Go transpilers** for the portable-logic files
(anything not touching `REG_*`/asm/naked functions directly):

- [`cznic/ccgo`](https://gitlab.com/cznic/ccgo) — compiles C to Go as a real
  backend, not a toy: it's what produces `modernc.org/sqlite`, a Go port of
  SQLite's C amalgamation that's auto-regenerated from upstream C and used in
  production. Strong precedent that this tool can carry a large, real C codebase
  across to working Go mechanically.
- [`gotranspile/cxgo`](https://github.com/gotranspile/cxgo) — aims for more
  *readable*/idiomatic output than ccgo, at the cost of being explicitly
  experimental/less battle-tested.

Realistic expectation: these tools handle straight control-flow-and-math C well,
and choke on exactly the parts pokeemerald leans on heavily — packed bitfields,
X-macro-style code generation, direct hardware register access, inline asm. So
the plan isn't "run ccgo over `src/` and done" — it's:

1. Run ccgo/cxgo over the clearly portable files (`pokemon.c`, `random.c`,
   `battle_script_commands.c`'s control flow, most of `src/data/*.h` tables) to
   get a first-draft Go translation.
2. Hand-fix what doesn't transpile cleanly (bitfields, macros, anything touching
   the hardware seam above — which gets hand-written against the new backend
   regardless, not transpiled).
3. Treat the ccgo/cxgo output as a Stage 1 starting point, not a finished
   product — its output (like `modernc.org/sqlite`'s) is deliberately low-level/
   unidiomatic. That's fine for Stage 1; cleaning it up *is* Stage 2.

This should be piloted on one mid-size, hardware-agnostic file (e.g. `random.c`
or `pokemon.c`) before committing to it as the Stage 1 workflow — confirm output
quality and hand-fix cost on a real example before assuming it scales.

### Scripts, data, and tests in Stage 1

Unchanged from the earlier pass at this analysis:

- **Battle scripts** (`data/battle_scripts_*.s`) and **overworld event scripts**
  (`data/event_scripts.s`) are bytecode interpreted by `battle_script_commands.c`
  and `src/scrcmd.c` respectively. Port the two interpreters literally; re-express
  each script as Go data (opcode/arg slices) generated once from the existing
  `.s`, rather than porting the assembler toolchain.
- **RNG** (`src/random.c`) needs literal, call-order-exact translation if any
  output should match the original — this is the one place "close enough" isn't
  good enough.
- **Data tables** (species/moves/items/abilities/learnsets/trainers/encounters/
  maps/text) are the easiest win: mechanical conversion to Go structs/slices or
  `go:embed`ed JSON, testable immediately, no logic to get wrong.
- **Test suite**: port the GIVEN/WHEN/SCENE/THEN DSL to drive the Stage 1 Go
  engine directly instead of mGBA + framebuffer assertions. Build this *before*
  porting battle logic — it's the parity oracle that makes "one-to-one" a checked
  claim instead of an assumption, for both Stage 1 (vs. the C original) and
  Stage 2 (vs. Stage 1).

## Stage 2: cleanup pass

Only once Stage 1 passes the ported test suite. Refactor without changing
behavior:

- Break up monoliths (`battle_script_commands.c` is 18K lines; `pokemon_storage_
  system.c` is 10K) into proper Go packages by responsibility.
- Replace GBA-save-constrained tricks that Stage 1 carried over unchanged for
  fidelity (box Pokémon substruct shuffle/encryption, sector wear-leveling
  remnants) with idiomatic Go — this is where those get actually dropped, once
  Stage 1 has proven the *logical* save fields are right independent of the GBA
  packing scheme.
- Turn the bytecode-VM script system into either a cleanly-typed VM or native Go
  functions/data, as preferred — Stage 1 keeps it VM-shaped for fidelity, Stage 2
  is where that's revisited.
- Introduce a real renderer/backend interface instead of Stage 1's direct
  Ebitengine calls, if platform flexibility (e.g. swapping backends per platform)
  turns out to matter.
- Normal Go idioms throughout: proper error returns instead of C sentinel values,
  package-scoped state instead of global EWRAM-style variables, exported types
  with real names instead of `u8`/`u16`/`u32` aliases mirrored from C.
- Re-run the *same* ported test suite from Stage 1 throughout — it's now
  asserting Stage 2 against Stage 1's proven-correct behavior, not against the C
  original directly.

## Scope, by the numbers (this checkout)

- `src/*.c`: 349 files, ~506K lines
- `src/data/*.h` (data tables included by the above): 153 files, ~514K lines
- `include/*.h`: 412 files, ~62K lines
- `data/*.s` (battle/overworld/field scripts + misc tables): 49.8K lines
- `test/*.c`: 937 files, ~99K lines
- Graphics assets: ~18.2K files; JSON data files: 520; map directories: 519
- `battle_script_commands.c` alone (the battle bytecode interpreter): 18K lines

Order of magnitude: **~1.1M lines of C/data, ~18K graphics assets, ~500 maps.**
Multi-year scope for a small team if genuinely one-to-one (every move effect,
every overworld interaction, every menu) even with transpiler assistance on the
data/logic-heavy portion. Not a scope for a single session or a single agent run
— plan it as an incremental, subsystem-by-subsystem effort with the test suite
as the gate at every step, per the phased order in the previous section.

## Open questions

- **Ebitengine vs. go-sdl2** for the hardware-seam backend — recommended
  Ebitengine above, but worth an explicit decision before the PPU package exists,
  since it's not a cheap thing to change later.
- **ccgo/cxgo pilot**: worth spending a small trial on one file before assuming
  it's part of the Stage 1 workflow — confirm real time-savings vs. hand-fix cost.
- Is bit-exact RNG/damage-roll parity with the real ROM required, or is "plays
  the same" sufficient? Affects how pedantic Stage 1 needs to be in the RNG/damage
  math specifically.
- Mobile as a real target, or just "possible later because Ebitengine supports
  it"? Affects how much Stage 1 needs to think about touch input/UI scaling
  up front vs. deferring it.
- Full `pokeemerald-expansion` feature set (this fork, with its large
  config-gated surface in `include/config/*.h`) or upstream `pokeemerald` only,
  with expansion features added incrementally after Stage 1 lands?
