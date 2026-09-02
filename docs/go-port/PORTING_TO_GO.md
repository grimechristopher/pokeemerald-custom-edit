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

**End-state requirement: zero C.** Not "C game logic wrapped in a Go shell" —
every line of C (and the one hand-tuned assembly mixer) is expected to end up as
Go, with no residual C toolchain, no cgo, no linked C library, in the finished
product. This is stronger than the earlier draft of this doc assumed, and it
rules out one of the two backend options below outright — see
[Dependency policy](#dependency-policy-zero-c). "Ugly, C-shaped Go" from Stage 1
still satisfies this (it's real Go, no C compiler involved to build or run it);
it's Stage 2's job to make it idiomatic, not to finish converting it — that
conversion is done by the time Stage 1 exists.

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

## Keeping in sync with upstream

This repo already tracks the target directly — `git remote -v` shows an
`upstream` remote at `rh-hideout/pokeemerald-expansion`, and local `master` is
currently 203 commits behind it. The goal here isn't a one-time snapshot port:
it's a Go codebase that keeps tracking upstream — the base ROM plus the
community's ongoing edits — as it continues shipping new Pokémon, moves,
trainers, features, and fixes. That's a real constraint on *how* Stage 1 gets
built, not just what it produces once.

### Why "one-time port" and "stays in sync" pull against each other

A hand-translated Go file has no mechanical relationship back to the C file it
came from. The next upstream commit touching that C file has no way to
propagate to it. Multiply that by ~1,100 source files and every future upstream
release becomes a manual re-diff-and-reapply exercise across a codebase that's
no longer even in the same language — precisely the kind of drift that makes
ports go stale and get abandoned within a year.

### The fix: separate generated code from hand-written code, from day one

Treat this the way any project with a codegen step does (protobuf, sqlc,
`modernc.org/sqlite` itself): a hard line between code that's **regenerated
wholesale from upstream C on every sync** and code that's **hand-written once**
and never touched by the generator.

| Regenerate on every sync | Hand-write once, wrap the generated layer |
|---|---|
| Data tables (species/moves/items/abilities/trainers/encounters/maps/text) — mechanical, low-risk to regenerate | Hardware seam (`ppu`/`apu`/`input`/`save`/`bios`) — no C equivalent exists to regenerate from |
| Script bytecode data (battle scripts, event scripts) — mechanical | Stage 2 idiomatic refactors, but only for logic that's genuinely stable upstream (RNG, core stat/damage formula shape) |
| Game logic run through the ccgo/cxgo pass, kept in a clearly marked generated package per subsystem | Anything Stage 2 substantially restructures — accepted case-by-case as needing manual re-merge on upstream changes, not a default |

Concretely: on each sync, re-run the generator pipeline (data-table extraction +
ccgo/cxgo for logic) against the new upstream tree, diff the regenerated output
against what's committed, and hand-reconcile only what didn't transpile cleanly
(new macros, new hardware touches). This is the same workflow
`modernc.org/sqlite` uses to track upstream SQLite releases automatically —
directly relevant here since it's the same tool.

### What this means for Stage 2

Stage 2's idiomatic cleanup is exactly the thing that breaks regeneration for
whatever it touches — that's the real cost of Stage 2, not just engineering
time. Fine to pay for subsystems upstream rarely touches (the RNG algorithm,
core formula shapes). Expensive for subsystems upstream changes constantly
(species/move/trainer/encounter data, new move effects) — those should stay
behind the generated/regenerable seam even after Stage 2 lands elsewhere, or
Stage 2 should target a *wrapper* layer around the generated data rather than
replacing the data layer itself. Worth deciding per-subsystem, not by a single
blanket rule, when Stage 2 planning actually starts.

### Practical sync cadence

Upstream ships versioned releases (this repo's own `INSTALL.md` documents the
update path: 1.6.2 → 1.7.4 → 1.8.3 → 1.9.4 → 1.10.3). Syncing on that same
cadence — re-run generators against each tagged release, run the ported test
suite to catch behavioral drift, hand-fix whatever didn't transpile — is a
sustainable rhythm, rather than chasing every individual upstream commit.

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

**Ebitengine, decided — not just recommended.** The "zero C" end-state rules out
the alternative:

- **Ebitengine.** Pure Go, no cgo, single static binary — required by the
  zero-C policy. Cross-compiles to Windows/Linux/macOS from one codebase, and
  already has first-class Android/iOS (via `gomobile`) and browser/WASM support,
  covering the same platform list pokeemerald-multiplatform targets (plus more)
  without a parallel Gradle/NDK build path. Trade-off: pokeemerald-multiplatform's
  SDL2 code (aspect-ratio math, audio-timing fixes) can only be used as a
  *reference* to reimplement against, not reused directly.
- ~~go-sdl2 (cgo bindings to real SDL2)~~ — **rejected.** It's the closer match
  to the precedent project's own code — its rendering/scaling math and audio
  backend choices would translate almost line-for-line — but it links a real C
  library through cgo by definition. That's exactly the dependency the zero-C
  end state rules out, so it's not on the table regardless of how much easier a
  first pass it would be.

### Dependency policy: zero C

The Ebitengine-over-go-sdl2 call above is one instance of a general rule for the
whole Stage 1 dependency list, not a one-off: **every third-party Go package
pulled in must be pure Go (no cgo, no linked C library, no C build step).**
Concretely, watch for this when picking libraries for anything the C original
handled through a C library or its own hand-written C:

- Audio decoding (music/SFX sample data) — use pure-Go decoders (e.g.
  `hajimehoshi/go-mp3`, `jfreymuth/oggvorbis` — both cgo-free, and both already
  the kind of library Ebitengine's own audio examples use) rather than anything
  wrapping `libvorbis`/`libmp3lame`/etc.
- Image decoding for graphics assets — Go's standard `image/png` etc. are
  already pure Go; no need to reach for `libpng` bindings.
- Compression, if any assets are compressed at rest — Go's standard `compress/*`
  packages are pure Go; don't reintroduce `zlib`/cgo equivalents.
- Ebitengine itself — confirm cgo-free for every actual target platform before
  committing (it is, via `purego`-based OS bindings on desktop; worth a final
  check specifically for whatever mobile toolchain gets used, since mobile
  build paths are where a stray cgo dependency is most likely to sneak back in).

None of this is exotic — it just needs to be a checked constraint when adding a
dependency, not an assumption, since "port everything to Go" quietly fails if
the last mile depends on a wrapped C library.

The `tools/` C programs (`gbagfx`, `mid2agb`, `gbafix`, `compresSmol`,
`patchelf`, etc.) don't need porting at all under this plan — they're part of
the GBA ROM build pipeline, which the Go executable target doesn't use. They
stay C for as long as this repo still also builds the GBA ROM for the existing
`pokeemerald-expansion` audience; they're simply not part of what "zero C"
applies to, since they never run in the shipped Go binary.

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

### Stage 1 package layout

A Go module laid out with the hardware seam isolated into its own packages, and
the logic side kept close to the existing `src/` grouping so files map back to
their C originals one-to-one during translation:

```
go-port/
  cmd/pokeemerald/        # entrypoint: Ebitengine game loop, ties packages together
  internal/
    ppu/                  # virtual PPU: backgrounds, OAM/sprites, palettes, blend/window
    apu/                  # audio: m4a.c sequencer logic (ported ~1:1) + a new Go mixer
                           #   replacing m4a_1.s (hand-tuned asm has no literal target)
    input/                # keypad polling -> Ebitengine input
    save/                 # save read/write; logical SaveBlock1/2 fields, no sector/flash emulation
    link/                 # link-cable stub (future networked multiplayer), not implemented Stage 1
    bios/                 # BIOS/SWI call replacements (VBlankIntrWait etc. -> plain Go calls)
    battle/               # battle_main, battle_util*, battle_script_commands (VM), battle_ai_*
    overworld/             # event_object_movement, field_effect, scrcmd (VM), tv, contest, dome...
    pokemon/                # pokemon.c data model: stats, EVs/IVs, evolution, daycare
    scripts/                 # battle-script/event-script bytecode data + the two VM interpreters
    data/                    # generated: species/moves/items/abilities/trainers/encounters/maps/text
    menu/                    # party_menu, pokedex, easy_chat, slot_machine, pokemon_storage_system...
    rng/                     # random.c, literal port, call-order preserved
  assets/                    # go:embed graphics/audio/text extracted from original, converted once
  test/                       # ported GIVEN/WHEN/SCENE/THEN DSL + a Go-native test runner
  tools/gen/                   # one-off generators: C headers/json -> Go data, .s scripts -> opcode slices
```

`internal/ppu`, `internal/apu`, `internal/input`, `internal/save`, `internal/
link`, and `internal/bios` are the hardware seam (new code, see above); every
other `internal/*` package is a translation target with a specific C-file group
it corresponds to, which is what makes it possible to check Stage 1 progress
file-by-file against the original tree.

### Stage 1 size estimate

Grounded in this checkout's actual line counts (see [Scope](#scope-by-the-numbers-this-checkout))
and in pokeemerald-multiplatform's own hardware-seam code as a real data point —
its SDL2/win32/BIOS/DMA/audio-sink layer (excluding the experimental voxel
renderer, its mod system, and vendored cJSON) is **~220 KB of C, roughly 6-8K
lines** for three platform backends at once. A single Ebitengine backend should
land in the same range, not larger.

The dominant swing factor isn't the seam, though — it's **how data tables are
represented**, since they're over half the codebase by line count:

| Component | Source size | As literal Go source | As embedded assets (recommended) |
|---|---|---|---|
| Game logic (`src/*.c`, translated ~1:1) | 506K C lines | ~450-610K Go lines | same |
| Headers → Go type/const decls | 62K C lines | ~15-30K Go lines (no separate header file needed) | same |
| Data tables (`src/data/*.h`) | 514K C lines | ~500-550K Go lines (struct literals) | **~5-15K Go lines** (schema + loader; data itself becomes `go:embed`ed JSON/binary, not Go source) |
| Script bytecode data (`data/*.s`) | 49.8K lines | ~45-55K Go lines (opcode slices) | **~2-5K Go lines** (loader; data becomes an asset) |
| Hardware seam (new, not translated) | ~6-8K C lines (precedent) | ~5-10K Go lines | same |
| Test suite (`test/*.c`, DSL) | 99K C lines, 937 files | ~80-100K Go lines | same |

**Totals: ~1.1-1.3M lines of Go if data stays as literal Go source, vs. ~650-800K
lines of Go code (plus the same data now living in non-code asset files) if data
is embedded instead.** The earlier recommendation in this doc (data as
`go:embed`ed JSON, not Go source) is what keeps Stage 1 in the smaller range —
worth confirming as a decision, since it roughly halves the line count that
needs writing/reviewing as code, without changing what "one-to-one" means
behaviorally (the data is still exactly the same data, just not expressed as Go
struct literals).

Either way, this is **not a number a single effort tackles at once** — it's the
same order of magnitude as the original C project, which itself represents
years of decompilation and hacking-community work. The phased order earlier in
this doc (interpreters and data first, subsystem-by-subsystem with the test
suite gating each step) is what makes a number this size tractable at all.

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

- ~~Ebitengine vs. go-sdl2~~ — resolved: Ebitengine, per the zero-C policy.
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
