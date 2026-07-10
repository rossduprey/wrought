# Runbook

How this project is worked on. Not how it works — that is `DESIGN.md`, and not what it claims — that is `README.md`. This file is for whoever picks the work up cold, which so far has always been a language model with no memory of the last session.

It is deliberately not named `CLAUDE.md`. It is not meant to load itself into every conversation. Somebody has to point at it, on a day they have decided to work on this.

---

## Build

Debian 13. `g++ 14.2.0`, `GNU Make 4.4.1`. No dependencies, no build system beyond `core/Makefile`, no package to install. C++17, and `-Wall -Wextra -Wpedantic` is not decoration — the compiler has caught real defects here.

```
cd core
make test          # 85 assertions. Must print "ok (0 failures)".
make ratchet-run   # a measurement, not a test. Allowed to come out badly.
make pan           # play it
make clean
```

Everything is header-only in `core/`. There are exactly three translation units and each one is its own program.

Two remotes. `origin` is GitHub over SSH (`rossduprey/wrought`), `gitea` is the LAN mirror over HTTPS with credentials embedded in the remote URL. **Push to both**, always, in that order. The LAN mirror is not a backup — it is where the issue tracker's twin lives, and a mirror that has drifted is worse than no mirror.

---

## What is in `core/`

Read them in this order. Each one only knows about the ones above it.

| file | owns |
|---|---|
| `phase_table.h` | Ten minerals, four Wentworth size bins. Densities, susceptibilities, aspect ratios. Every number `UNVERIFIED` or `AUTHORED`. |
| `substance.h` | `Substance`: a `[phase][size]` mass matrix, twice — free grains and composites. `grade()`. No position, no vertical dimension, no volume. |
| `settling.h` | Terminal velocity from a force balance, solved as a damped fixed point. Water properties from temperature; grain shape from the aspect ratio. `free_velocity(p, s, T)` is the number the whole project turns on, and the long comment on what a bin diameter *means* is the most load-bearing paragraph in `core/`. |
| `separate.h` | The Tromp partition, `9^σ` imperfection, screening, crushing, closed circuit, the bed (`exposed()`), `skin_depth()`, `recovery()`. |
| `magnetic.h` | The lodestone. A force balance on `magnetic_susceptibility` — `L = χ·B∇B/(μ₀ρg)`, grain volume cancels. Separates magnetite from everything on an axis orthogonal to the pan. Its two authored magnet numbers are asserted **not to matter**, because magnetite's χ/ρ is ~1375× the next phase's. |
| `smelt.h` | The bloomery. The first process that is chemistry, not mechanics — so it is a **tabulated reaction**, not a force balance. An element ledger (Fe/Si/O/C/other, from formulae and standard atomic weights) that balances across ore + charcoal → bloom + slag + gas. Its one finding is derived, not authored: with no flux, silica leaves as fayalite and eats **1.859 kg Fe per kg SiO₂**, so a charge smelts only above Fe/SiO₂ = 1.859. |
| `levigate.h` | The batch decant. Its partition is a ramp and it is exact — there is no authored sharpness in this file. |
| `fire.h` | The two bridges from a body of clay to a tool: grit → sharpness, clay → vessel. The first is **derived** now — the log law over a rough bed — and keeps one bounded shape factor where it used to keep an invented functional form. The second is still invented end to end. Both are issues. Read the "two misplacements" note at the foot of its header before you touch `fire_pan`. |

`test_separation.cpp` is the suite. `pan.cpp` is the playable slice. `ratchet.cpp` is neither — it is an instrument, it prints tables, and it exists because `DESIGN.md` asserted something without ever having run it.

---

## The rules

**An uncited number is a bug.** This is `CONTRIBUTING.md`'s rule and it governs the code as much as the design. A number that came from a language model's memory ships as `UNVERIFIED` and gets a tracked issue. A number that was chosen by a human because it felt right ships as `AUTHORED` and gets a tracked issue. The distinction matters: an unverified number has a correct value somebody could look up, and an authored one does not.

**Distinguish authored from derived, out loud, in the comment.** The single most valuable habit here. `settling.h`'s clay lower bound of 0.2 µm is authored, and the file says so; the three-hundred-fold settling gap that falls out of it is derived, and nobody chose it. When a finding rests on a bridge, name the bridge.

**A measurement is allowed to come out badly.** `make ratchet-run` was written to confirm the design document's central mechanism and it refuted it instead. That is the best day this project has had. Do not fix an instrument that is telling you something you did not want to hear.

**When a claim turns out to be wrong, the wrong claim stays.** Written next to the right one, dated, with what caused the error. There are two preserved in `levigate.h`, one in `README.md`'s claim #2, one on `README.md`'s front page, and one each in `fire.h`, `ratchet.cpp`, `test_separation.cpp` and `DESIGN.md`. They are not self-flagellation; they are the only record of *why* the current sentence is worded the way it is, and without them somebody will helpfully revert it. Two of them are wrong claims about wrong claims.

**Grade and recovery are always reported together.** One of them alone is a lie by omission. Reporting either as a single score invites optimising it, and the composition vector — not a score — is the state.

**Teach the tool, never the wisdom.** Any interface may explain how an input maps to an action. No interface may state the strategy that emerges from using it. If the player can be told the answer, the simulation was decoration.

---

## How the work actually goes

1. Probe in the scratchpad. Throwaway `.cpp` files that link the headers and print a table. Do not put a probe in the repo; do steal its findings.
2. If it found something, decide whether it is a *test* (an invariant that must hold) or a *measurement* (a number we want to watch). Tests go in `test_separation.cpp`. Measurements get their own program.
3. Let the result contradict the design document. Then change the design document, and say in the commit that it lost.
4. New authored numbers get an issue before the commit that introduces them lands, tagged `authored-number` or `unverified-number`.
5. Commit, push to `origin` and `gitea`.

Commit messages are lowercase, `area: what changed`, and they are allowed to be about what was learned rather than what was typed — `core: levigation, and the ratchet does not turn`.

---

## Where we are

Era 0 (hands, pan, sluice, lodestone) and Era 1 (levigation, fired pot) are implemented and measured, and the door into Era 2 is open: the bloomery smelts, and iron falls out of it (`smelt.h`, 2026-07-10). Nothing past the first bloom exists — no bar, no forge, no copper.

**This section said, on 2026-07-09, that #13 was "the single most dangerous uncited number in the project — it can reverse a finding rather than refine one." On 2026-07-10 it was done, and it did.** One of the four settled results below is now struck through. Read that as encouragement: the paragraph was right, the work was worth prioritising, and the way to find out was to go and do the arithmetic. **#10 was done the same day, for the same reason, and it reversed a different thing than anyone expected.** Of the three results that remain, one is now derived rather than provisional:

- **Progression is a staircase, not a ratchet.** You do not refine a tool into a better tool; you invent a different mechanism with its own irreducible sharpness. *This used to say "overturned if the grit → sharpness bridge in `fire.h` has the wrong functional form — its magnitude is safe by three orders of magnitude, its exponent is not."* **The bridge did have the wrong functional form, and the staircase gained margin rather than losing it.** #10 replaced it with the log law over a rough bed: the exponent is `1 + 1/ln(15·d_cut/d_grit)` ≈ 1.11, *super*-linear rather than the feared `√(d/h)`, and roughness cannot blur the pan past 3.4% at any grit size whatever. σ is 0.5500 at every generation to nine places. **Within this model nothing is left that could overturn it:** a pan's sharpness has exactly two inputs — the operator's wrist (#5, which sets a step's height, not whether there are steps) and the floor's roughness (derived). Restoring the spiral now requires a *new mechanism* by which finer clay makes a sharper pan, and none is proposed. *(It also survived #13 untouched, which was the obvious thing to fear and did not happen — a clay platelet has no opinion about sand.)*
- **Levigation authors nothing.** Its imperfection is exactly 3.0 for every vessel, charge and wait, and even the pour fraction is derived. Nothing to overturn — it is algebra. *(Survived #13: the algebra never moved. Every number it stood on did.)*
- ~~**Era 1 has a lodestone-shaped hole.** Levigation cannot separate clay from clay-sized quartz; the velocity ratio is 1.031×, the same 1.03× that hides magnetite in hematite.~~ **Overturned 2026-07-10 by #13.** The 1.031× was the ratio between two *spheres* of equal diameter and unequal density; kaolinite is a platelet and the real ratio is **6.977×**. Levigation divides the clay bin, reaches grade 1.000 given patience, and has a grade/recovery curve like every other separator here. Deflocculation (**#15**) survives as an *improvement*, not a *requirement*.
- **Throughput is a bigger shovel.** More mass per hour is not progression. A bigger pot is, because it moves the curve outward at matched grade. This distinction was got wrong once already, in the direction of thinking the pot's loop was illegal. *(Survived #13, but its old proof did not: "same grade, more recovery" was only sayable while grade was pinned. The pot now wins at every matched grade, which is the real test and the one §2a specifies.)*
- **Iron has a wall, and it is 1.859.** *(Added 2026-07-10 by `smelt.h`.)* A fluxless bloomery loses 1.859 kg of iron to fayalite slag per kg of silica — two atomic weights, nobody's choice — so a charge smelts only above Fe/SiO₂ = 1.859. Panned sand assays 0.12 and washes no higher than ≈ 0.5, so **the pan cannot make smeltable ore.** This is the concrete form of "possibility is not the gate, efficiency is," and it is why the lodestone is not romantic decoration: crush-and-magnet is the only Era-1 path across the wall. If any of `phase_table.h`'s densities or `substance.h`'s `COMPOSITE_TARGET_FRACTION` move, the exact ratios shift but the wall does not — it is stoichiometry, not fixture.

Three habits earned their keep on 2026-07-10 and are worth stating as rules rather than as anecdotes.

**An uncited number can be load-bearing for a conclusion in a different file, and nothing in the code will tell you which one** — the velocity ratio that Era 1 rested on was a property of the word "sphere", which nobody had ever written down as an assumption.

**A correction can be wrong.** `levigate.h` carried a dated, self-flagellating note admitting its algebra had been too clever about vessel depth; the algebra was right, and the note was covering for a defect one file over. Both stay, both dated. Do not assume the most recent sentence is the true one.

**A lesson learned in one file does not propagate to the next one by itself.** `separate.h` worked out on 2026-07-09 that *a separator has two misplacements, not one*, and rewrote `screen()` around it. `fire.h` was making the identical mistake about pot floors — one authored line, one function away, charging the cost of a grain that never moved to the blur of the grains that did — and it survived a full day and a passing test suite. When you fix a modelling error, grep for the shape of it, not for the symbol.

**A step's dependencies are not always the ones the plan drew.** This very section said, right here, that smelt "is downstream of nothing." It is downstream of *two* things — the lodestone and the crush — and the way that surfaced was a probe that tried to feed a bloomery from a pan and got a glassy slag. The plan's dependency graph is a hypothesis like any other; the arithmetic is what actually knows the edges. Build the probe before you believe the ordering.

The next piece of work is **bloom → bar → tool**: consolidating the spongy bloom and forging it, which is Era 2 proper and the first mechanics in the project that are *deformation*, not separation — genuinely new territory, downstream of everything, able to invalidate nothing already written. Everything else open is a set piece (**#15**, deflocculation), an unbuilt model (**#19**, hiding–exposure), or a citation chore (**#1, #2, #5, #7, #12, #16, #17, #18, #20, #21**). Note **#21** and **#20** as this session's debts: the bloomery's slag chemistry and reducible set are simplified, and the lodestone's and the `IRON` phase's numbers are authored — none of which can reverse the two findings they sit under (the fayalite wall and the orthogonal magnetic axis), both of which are stoichiometry and susceptibility ratios, not fixture.

---

## Open, and written down nowhere else

`DESIGN.md` § "Open decisions — Ross picks" holds the *project* decisions — build host, scope, licence, canonical host — and most are resolved. It does not hold these. These were carried in conversation across several sessions, which is not a place things survive. They are Ross's to answer and none of them has been.

- **Does the three-clause test go into `DESIGN.md` as a section explicitly owned by Ross**, the way §2a is? Asked twice, never granted.
- **Does "teach the tool, never the wisdom" become a stated rule** rather than a habit that happens to be followed?
- **The §2b experiment.** Two actors, two processes, one clock; find out what breaks. Costs nothing and needs no engine.
- **The re-panning verb.** Throwing tailings in the river is wrong — real panners re-pan them. The verb is free and has never been picked up.
- **Is `pick` a tax or a decision?** If it is a tax, it should not be a verb.
- **How does a real panner's *tilt* become a mouse gesture?** This decides whether the pan is playable at all, and it is the only question here that could stop the project.

When two fixes both look right and trade off against each other, say so and let Ross choose. Do not quietly take one.
