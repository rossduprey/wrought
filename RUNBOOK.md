# Runbook

How this project is worked on. How it works → `DESIGN.md`. What it claims → `README.md`. Where it stands → `STATUS.md`. Reversals → `CORRECTIONS.md`.

Not named `CLAUDE.md` on purpose: it does not load itself. Someone points at it on a day they have decided to work on this.

## Build

Debian 13, `g++ 14.2.0`, `GNU Make 4.4.1`. No dependencies, no build system beyond `core/Makefile`. C++17; `-Wall -Wextra -Wpedantic` has caught real defects.

```
cd core
make test          # must print "ok (0 failures)"
make ratchet-run   # a measurement, not a test — allowed to come out badly
make pan | dig | breaker | furnace | carry   # the playable slices
make clean
```

Header-only. Seven translation units, each its own program. `origin` is GitHub (`rossduprey/wrought`), the single source of truth: fork + PR, maintainers push to `origin main`. (Maintainer push path lives in an untracked local file.)

## Code map

Read in order; each file knows only the ones above it.

| file | owns |
|---|---|
| `phase_table.h` | Minerals × Wentworth size bins. Densities, susceptibilities, aspect ratios. Every number `UNVERIFIED` or `AUTHORED`. |
| `substance.h` | `Substance`: a `[phase][size]` mass matrix twice — free grains and composites. `grade()`. No position or volume. |
| `settling.h` | Terminal velocity from a force balance, damped fixed point. Water props from temperature, shape from aspect ratio. `free_velocity(p,s,T)`. Read the bin-diameter comment. |
| `separate.h` | Tromp partition, `9^σ` imperfection, screening, crushing, closed circuit, the bed (`exposed()`, `skin_depth()`, `shelter[]`), `recovery()`. |
| `magnetic.h` | The lodestone. Force balance on `magnetic_susceptibility`; separates magnetite on an axis orthogonal to the pan. |
| `smelt.h` | Bloomery + furnace, tabulated reactions on an element ledger (Fe/Si/O/C/S/Cu/Sn). `smelt()` (iron), `smelt_copper()`/`smelt_tin()` (pours), `roast()` (desulfurize chalcocite). Reads melting points for sponge-vs-pour. |
| `forge.h` | Consolidation (bloom→bar) and shaping (bar→tool), metal-neutral. `cast()`, `alloy()` (bronze). Hardening on each metal's recryst floor. |
| `levigate.h` | Batch decant (exact ramp) and `flocculate()` (divides clay from clay-sized quartz by surface charge). |
| `fire.h` | Grit→pan-sharpness (derived, log law over a rough bed) and clay→vessel (invented). Read the "two misplacements" note before `fire_pan`. |
| `geology.h` | First spatial model: composition field `f(x,y,tier)`. `sample()` / `dig_column()`. Depth-tier changes the mineral; `Origin` (HARDROCK/PLACER) and `liberation_bin()` set what comes up free. `DEPOSITS` far apart → co-location is geometry. Layout authored (#28); tests assert field shape only. |

`test_separation.cpp` is the suite. `pan/dig/breaker/furnace/carry.cpp` are the playable slices; `ratchet.cpp` is an instrument.

## Rules

- **An uncited number is a bug.** An LLM-memory number ships `UNVERIFIED` + an issue; a human-chosen one ships `AUTHORED` + an issue. The distinction: an unverified number has a correct value someone could look up; an authored one does not.
- **Distinguish authored from derived, out loud, in the comment.** When a finding rests on a bridge, name the bridge.
- **A measurement is allowed to come out badly.** Do not fix an instrument telling you something you did not want to hear.
- **A wrong claim is kept, dated — in `CORRECTIONS.md`, not inline.** The docs state current truth only.
- **Grade and recovery are always reported together.** State is the composition vector, never a score.
- **Teach the tool, never the wisdom.** An interface may explain input→action; it may not state the strategy that emerges. If the player can be told the answer, the sim was decoration.

## Editing documents

These are technical documents. Terse, load-bearing prose — not story hour.

- **Edit the changed span, nothing else.** Never delete a paragraph and retype it with the same words back to alter one clause. Match the smallest unique string and replace only what changed. Re-typing unchanged text is wasted tokens and wasted work time.
- **Additions are precise and focused.** Each new sentence must carry a fact the reader does not already have. Say it once. Do not restate the surrounding text in new words, do not pad with framing, do not narrate what the change does.
- **Cut, don't accrete.** If an edit can be shorter and still complete, make it shorter. A finding that needs three clauses gets three, not a paragraph.

## How the work goes

1. Probe in the scratchpad (throwaway `.cpp` that links the headers and prints a table). Steal the finding; do not commit the probe.
2. Decide: invariant → a test in `test_separation.cpp`; number to watch → its own program.
3. Let the result contradict `DESIGN.md`. Then change `DESIGN.md`, log the reversal in `CORRECTIONS.md`, and say in the commit that it lost.
4. New authored numbers get an issue before the commit that introduces them lands.
5. Commit lowercase `area: what changed`, push to `origin`. Settled and not to be re-verified before every push: identity is `ross <rossduprey@gmail.com>`, `origin` is SSH `git@github.com:rossduprey/wrought.git`, branch is `main`. Just `git add`/`commit`/`push` — it has never failed; let it error if it ever does instead of pre-checking.

## Open questions — Ross's to answer

Carried in conversation, not resolved:

- Three-clause test as a `DESIGN.md` section Ross owns, like §2a?
- "Teach the tool, never the wisdom" as a stated rule, not just a habit?
- §2b: two actors, two processes, one clock — find what breaks.
- The re-panning verb (re-pan tailings, not dump them).
- Is `pick` a tax or a decision? If a tax, it should not be a verb.
- How does a real panner's tilt become a gesture? — the one question that could stop the project.

When two fixes both look right and trade off, say so and let Ross choose.
