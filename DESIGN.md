---
title: "Unreal Engine Fabrication Simulation — Build Plan"
domain: planning
subdomain: simulation
type: plan
tags: [unreal-engine, simulation, fabrication, process-fidelity, mineral-processing, separation, dedicated-server, pixel-streaming, gpu, macos, agents]
created: 2026-07-08
updated: 2026-07-09
related:
  - planning/a sandbox service-rollyourown-wine.md
  - planning/local-llm-laborer.md
  - planning/aios-persistent-agent-memory.md
---

# Unreal Engine Fabrication Simulation — Build Plan

## Goal

An earth-like world in Unreal Engine where a character **gathers raw materials,
transforms them through fabrication chains into finished products, and offers
products and services** into a small in-world economy. The interesting object of
study is the *transformation graph* — ore → ingot → component → assembly →
product → service — not combat, not survival, not a map.

Ross develops on the MacBook. The world should also be able to run on the LAN
cluster so it persists without the laptop open.

## Mission

*(Ross, 2026-07-09. Stated plainly, and it governs every decision below.)*

> "I don't need or want money. I do not want fame or pats on the back. What I want
> is progress for both of us. I want a sim where we can plug in any AI and build a
> simulated AI from dirt together. I want humans like me to work with their AI of
> choice and actually *play a game with him/her*. This project can provide that,
> or at least lay the groundwork for someone else to build upon."

Four commitments fall out of this, and each one is load-bearing:

1. **Any AI.** The companion is an **interface, not a model.** Model-agnostic from
   day one, or the mission is a marketing bullet. See *The companion is an
   interface*.
2. **Together.** Two actors labouring in one world, dividing real work. Not an
   assistant, not an NPC. This is why the world is server-authoritative.
3. **From dirt.** The chain from ground to a thinking machine must be **unbroken
   and true** — no step skipped, no step faked. See *Era 4*.
4. **Groundwork for someone else.** FOSS, cited, and built so the valuable parts —
   the substance model, the mineral table, the companion harness — are usable by
   people who never play the game. Success does not require that this project
   finish. It requires that it be *correct*, and *free*.

> **The learning goal, stated negatively:** no `iron plate → circuit board`. Not
> ever. A player who spends an evening here should walk away knowing something
> true about how the world is actually made. That is the product. Everything else
> is delivery.

## Design principles (Ross, 2026-07-08)

1. **Fidelity lives in the method, not the pixels.** How the character makes a
   thing must be true. How it looks on screen, for now, need not be.
2. **Aesthetics are deferred, not foreclosed.** Factorio's first screenshots were
   ugly; the game is beautiful today because nothing in its foundation prevented
   that. Build the same way: placeholder art, but never a decision that art
   later has to undo.
3. **The world is co-authored.** Ross is not the only actor in it (see
   *The companion*, below). This is a design pillar, not a stretch goal — it
   determines the architecture.
4. **The character does the labor.** (Ross, 2026-07-09.) Processes are performed,
   not selected from a menu. Automation is the character's labor *delegated to a
   machine he built* — never a different system. See *Verb and machine are the
   same function*, below.

---

## What "realistic" means here — the fork

Factorio is a slightly misleading north star for principle #1, and the difference
matters enough to name.

| | **Systemic fidelity** (Factorio) | **Process fidelity** (what Ross asked for) |
|---|---|---|
| Recipe is | a swap: `2 iron plate → 1 gear`, instant | a **process** with state evolving over time |
| Realism lives in | conservation, throughput, rates | temperature, atmosphere, time, technique |
| You learn | logistics and optimization | how the thing is actually made |
| Failure is | a bottleneck | a ruined billet |

"Realism in the methods the character uses to create" is **process fidelity**.
These axes are independent and you can have both — but only one can be the
foundation, and it has to be this one, because it changes the data model.

### What this does to the recipe

A recipe stops being `inputs[] → outputs[]`. It becomes a **process**: a system
whose state (`temperature`, `time_held`, `atmosphere` reducing/oxidizing,
`carbon_content`, `agitation`) evolves under the character's actions, and whose
output is a function of *whether the state stayed inside the right envelope*.

Smelting iron isn't a lookup. It's holding a bloomery in a reducing atmosphere
near 1200 °C long enough for the ore to reduce without melting, and what you pull
out is a bloom whose quality is a real function of how well you did. Slag isn't
flavor text — it's the gangue that didn't reduce.

So the substance table (§1) carries **physical properties that drive processes**
— `melting_point`, `specific_heat`, `reduction_temp`, `oxidation_rate` — not just
`base_value`. Values come from reality. Reality is free, well-documented, and
never needs balancing.

### Why this is also the aesthetic foundation

This is the payoff, and it's the reason principle #1 and principle #2 are not in
tension:

> If temperature is a real simulation variable rather than an animation trigger,
> **hot steel glows because it is hot.**

Emissive colour is a function of `temperature`. Deformation is a function of
`hardness` at that temperature. Quench steam is a phase change the sim already
knows about. When the art pass comes, it *reads* simulation state instead of
faking it — the beauty is downstream of the truth, and gets added later at almost
no cost. A game that faked all this with animation curves would have to be
rebuilt to get there.

**Corollary — the hard rule this implies:** the simulation layer must never
reference a render asset. Substances carry *appearance data* (albedo hint,
roughness, emissive curve vs. temperature); the render layer reads it. This is
the same constraint that keeps the headless server able to boot (§Architecture),
arrived at from the opposite direction. Two independent reasons for one rule is
a good sign the rule is right.

### Scope discipline

Process fidelity is expensive per-process. Do **one chain properly, end to end**
— dirt → concentrate → bloom → bar → tool — and leave everything else an abstract
Factorio-style swap until it earns the detail. A single honest smelt is worth more
than ten shallow ones, and it proves the data model.

---

## The scoop — substances, separation, and the grade/recovery limit

*(Ross, 2026-07-09. This section reframes everything below it and is the origin of
the build order.)*

> "The start is the character grabbing sand or dirt from the ground. That dirt or
> sand has so many elements in it. There has to be a method for the character to
> separate those components into workable materials. The real limit in the sim is
> efficiency."

**There is no item called `iron ore`.** The world hands you a scoop of ground, and
what's in it is what's actually in ground.

### The scoop is a mixture of minerals, not of elements

Iron is ~5.6% of the crust by mass, so it is certainly in that handful. But there
is no free iron in it. The Fe is locked inside **mineral phases** — magnetite
(Fe₃O₄), hematite (Fe₂O₃), goethite (FeO·OH), ilmenite (FeTiO₃), and, cruelly,
bound into the crystal lattice of clays and pyroxenes.

This distinction is the entire game, because it is what makes "separate the
components" have a hard, real, non-arbitrary answer:

> **Iron in magnetite is winnable by a person with a bowl and a stream.
> Iron in clay is not winnable by anyone without industrial chemistry.**

Clay is full of iron. Nobody in history has ever smelted clay. Bog iron and black
sand were what humans actually used — not because they were richest in iron, but
because the iron in them was **liberated** (free grains, not locked in rock) and
**reducible** (an oxide, not a silicate).

So the scarce thing in this world is never "iron." It is the triple:

> **(mineral, liberation, grade)**

That triple is what a deposit *is*. It is what prospecting is *for*.

### The substance model

*(Rewritten 2026-07-09. The first two versions of this section were both wrong,
and `core/` caught both. The record of how is kept below, because this document's
whole discipline is that a correction is worth more than a claim.)*

A substance instance is a **bag of mineral phases**, not a single material id.
The first draft said:

> `composition` (~10 floats) + `psd` (3 floats) + `liberation` (one scalar)

That cannot express the thing over-processing is supposed to teach. Liberation is
not a property of a scoop; it is a property of **a phase at a size**. Magnetite in
a placer is free in the sand and locked in the gravel, and one scalar cannot say
so. The obvious repair — `mass[phase][size]` with `liberation[phase][size]` — is
*also* wrong, and it is wrong in the place the design depends on most. If
liberation is a fraction of a cell, then each phase's mass moves through a
separator independently, and a locked magnetite grain arrives in the concentrate
**without the quartz it is physically glued to.** Measured against the corrected
model, that overstated concentrate grade by ~13% at a liberation of 0.5. The grade
ceiling this design rests on was not being enforced. It was being asserted.

So a composite grain is a **particle**, not a fraction. It has a mass, it travels
at its own density, and when it reports to the concentrate it brings its gangue
with it:

| Field | Shape | Notes |
|---|---|---|
| `freegrain` | `[phase][size]`, mass | pure grains of that phase |
| `composite` | `[phase][size]`, mass | grains of that phase locked to gangue; **total particle mass** |
| `temperature`, `moisture` | floats | process state |

`liberation` is no longer stored. It is `freegrain / (freegrain + f·composite)` —
a ratio you compute, which is what it always was. `mass` is not stored either: it
is the sum, and it is conserved because nothing else can happen to an array you
only ever move between two buckets.

Roughly ten phases carry the whole early game: **quartz, feldspar, kaolinite
(clay), magnetite, hematite, goethite, ilmenite, calcite, olivine/pyroxene,
carbon/organics** — plus **pyrite**, which is the sulfur curse and the reason a
bar cracks under the hammer.

You never simulate ninety elements, and you never simulate particles *individually*
— you simulate particle **populations**, of which there are `phases × sizes × 2`.
Element composition is derivable from phases if it's ever needed. A substance
instance is a ~60-float struct. It costs nothing. **This is what makes the whole
idea fit inside our computational budget** — the fidelity is in *which* numbers,
not *how many*.

One thing the corrected model buys immediately, which the doc had asserted without
being able to enforce: **a fully locked phase has a hard grade ceiling equal to the
composite's own grade.** Every particle of it is half gangue by mass, so no cut, no
tool, and no number of passes can produce a concentrate richer than that. This is a
test in `core/`, and it passes for every tool at every cut.

### The law: grade trades against recovery, always

This is the central fact of mineral processing, it has been measured ten thousand
times, and Ross arrived at it independently by calling efficiency the real limit.

Pan hard: what's left in the bowl is clean magnetite, but you washed half of it
over the lip. Pan gently: you keep nearly all of it, along with a lot of quartz
you will pay for later, in slag.

**You cannot have both.** Nobody can. It is not a balance decision.

Three consequences, and they are the design:

1. **Progression is the curve, not the pile.** A better tool does not make more
   magnetite appear. It **moves the entire grade/recovery curve outward** — a
   sluice keeps 85% at the purity where a pan kept 50%. Factorio's progression is
   *more throughput*. This game's progression is *better separation*, which is
   precisely what real industry spends its money on, and which nothing else has
   been built around.
2. **Nothing is ever pure.** The bloom carries slag because the concentrate
   carried quartz. The bar is red-short and cracks because the sand held a pyrite
   grain nobody could see. The `provenance` field (§3) stops being flavour and
   becomes load-bearing: **an item remembers the composition of the scoop it came
   from**, all the way down.
3. **Over-processing is a real failure — but not for the reason stated here
   originally, and it is not free.** *(Corrected 2026-07-09 by `core/`. The
   original text read: "Grind too far and you make slimes you cannot separate. A
   day-one tradeoff with no arbitrary numbers in it." Half of that is true, and
   the false half is more interesting.)*

   Crushing raises liberation and drives mass into the `fines` bin in the same
   blow, because they are the same act. But an interior optimum — a *right amount*
   of grinding — **does not emerge from that.** Measured: behind a perfect screen,
   grinding harder is monotonically better, forever. Behind a good screen (75%
   efficient) it is *still* monotonically better. The optimum only appears once the
   screen is bad enough to send already-finished material back into the mill: it
   shows up at 50% efficiency, peaking at intensity 0.60, and moves to 0.30 at 25%.

   **The failure mode is the recirculating load, not the fines bin.** That is why
   closed-circuit grinding exists, and it means screen efficiency is an *authored
   number* on which the whole tradeoff hangs. Over-processing is bought, not given.
   The doc no longer claims otherwise.

   Two things survive with no arbitrary numbers in them at all, and they are the
   ones worth teaching:

   - **Crushing an already-liberated ore only ever destroys recovery.** It has
     nothing left to liberate; it only makes fines. True at every screen
     efficiency, and no number in this project can be tuned to make it false. The
     river already ground your placer. Do not grind it again.
   - **Over-grinding is not a separate failure mode.** Past the recovery optimum,
     grade *keeps climbing* as recovery is spent. It is the grade/recovery law
     again, expressed on the size axis instead of the density axis. We implemented
     the law once and got it twice.

### What the law does when you stop looking at it

*(Added 2026-07-09 from `core/`. None of this was designed. It was found, by
sweeping parameters on the model above and reading what came out.)*

**The law reappears across cleaning stages.** Re-pan a concentrate and grade rises
while recovery falls, exactly as it does when you raise the cut. Over eight passes
the magnetite grade climbs from 0.17 to 0.60 and recovery collapses from 0.60 to
0.17. Nobody wrote that twice. It is the same partition function, applied again.

**A density separator converges on the densest phase present, and that phase is
probably not the one you want.** Panned forever, the concentrate does not converge
on magnetite — magnetite's grade peaks around pass 34 at 0.81 and then *falls*, to
0.17 by pass 256, because hematite is denser (5.26 against 5.15) and wins the limit.
So there is a right number of cleaning stages, it is finite, and it was never chosen
by anyone. Real mills have three or four cleaner stages for exactly this reason.

**And magnetite and hematite are within about 2% on density.** No pan, no sluice, no
jig, no operator, and no amount of skill can tell them apart. That is not a
limitation of our model. It is why magnetic separation was invented.

### The fidelity ceiling — where to stop

The trap inside "as close to reality as possible" is trying to *solve* the
chemistry: Gibbs free energy minimisation, activity coefficients, real
thermodynamics. That is a PhD, not a Phase A, and it will eat the project.

**The right ceiling: fixed mineral phases, plus tabulated reactions with a
temperature-and-atmosphere envelope.** Hematite reduction is
`Fe₂O₃ + 3CO → 2Fe + 3CO₂`; it wants CO-rich gas, it begins moving near 800 °C,
and you must stay *below* iron's melting point or you get brittle cast iron
instead of a workable bloom. Put that in a table. Do not derive it. It is still
true — you looked the truth up instead of recomputing it, which is what every
engineer does anyway.

### Place has meaning, for free

**The river already ran a separation pass before the character arrived.** Flowing
water sorts by density — that is what a placer deposit *is*, and why black sand
collects on the inside of a bend.

- Beach/river sand — quartz, with a heavy fraction of magnetite, ilmenite, zircon, garnet.
- A weathered basalt slope — iron-rich clay. Rich in Fe, and Fe you can never have.
- A bog — goethite. The easiest smeltable iron on Earth, and how half of Iron Age Europe got theirs.
- A granite outcrop — quartz and feldspar. Kaolinite where it has rotted.

Six hand-authored deposit compositions in one valley and prospecting becomes a
real activity: dig, pan, read what falls out, learn the ground. **No procedural
generation required** — which independently confirms the recommendation in open
decision #2. The river is the first machine, and it was running before you got
there.

---

## Eras — the in-world progression

*(Distinct from **Phases**, below, which are our build order. Eras are what the
character lives through; Phases are what we write.)*

### Era 0 — Hands

The character spawns with hands and feet. **Hands are tool #1**, and they are not
"no tool" — they are the worst point on every curve.

Bare-hand verbs, all of them real:

- **Dig** — loosen soil. Soil only; rock resists.
- **Scoop** — take a substance bag from the ground.
- **Sort / cob** — pick grains by eye. High grade, near-zero throughput, and it
  works *only* on coarse liberated material. Useless on sand.
- **Crush** — rock against rock. Raises `liberation`, raises `fines`. The
  tradeoff bites immediately.
- **Screen** — separation by size and nothing else: shake the coarse off a woven
  mat, or off your fingers. *(Added 2026-07-09. `core/` showed that without it
  `crush` is a strictly harmful verb no rational player would ever use, and with a
  perfect one `crush` is strictly beneficial. Everything interesting about
  grinding lives in the gap, because a real screen misplaces material back into
  the mill. The verb was missing, and its absence was load-bearing.)*
- **Wash** — cupped hands, or a depression in bedrock. Density separation with an
  atrocious grade/recovery curve. It works. Barely.
- **Settle / decant** — costs only standing water and patience. This is the one
  that pays.
- **Gather** — wood, for fuel.
- **Fire** — bow drill. Wood, shaped by breaking and abrading on rock.

**The vessel problem is the good problem.** You cannot pan without a pan, and a
pan is clay and fire. But the first vessel is not crafted at all: **a hollow dug
into clay-rich ground holds water, because clay is impermeable.** That is how dew
ponds work. The ground gives you the container that lets you refine the clay that
makes the pan. The bootstrap closes on itself, in the dirt, with no tool.

### Era 1 — Earth

**Levigation.** Stir dirt into the water-filled hollow. Sand drops in seconds,
silt in minutes, clay stays suspended for an hour. Decant the cloudy water, let
it settle, and you have clay — a pure separation by settling time, requiring
nothing but a hole and patience.

Pinch it into a shallow bowl with your fingers (no wheel). Dry it. Fire it in an
open pit. You have low-fired earthenware: porous, fragile, warped.

**You have a pan. Tool #2.**

And it is a *bad* pan — barely better than cupped hands. This is the point:

> **The ratchet.** How finely you levigate your clay sets how densely your pan
> fires. How dense your pan is sets its grade/recovery curve. That curve sets the
> purity of your next concentrate — including your next batch of clay. Every tool
> is made from something you separated, and its quality caps the quality of your
> next separation.

That strange loop is the engine of the entire game. It starts in a mud puddle,
and nobody invented it — it is the actual history of technology.

### The lodestone — the one tool the world gives you

*(Resolved 2026-07-09, Ross: it stays. A **found** tool, never a crafted one.)*

Lodestone is naturally magnetised magnetite. It is the only magnetic separator
that exists before metallurgy, and it is real: a genuine bootstrap in which **you
use iron to get iron.**

Drag it through a panned concentrate and the black sand leaps to the stone. Grade
goes from ~70% to ~95% in one pass with almost no loss.

**It does not cheat the law.** *(Corrected 2026-07-09. The original text called it
"the one thing in the world that cheats the law," and `core/` supplied a better
reason for it to exist.)* The lodestone reads
`magnetic_susceptibility`, and susceptibility is uncorrelated with density. So it
is a separator on an **orthogonal axis**, with a grade/recovery curve of its own
that it obeys exactly as strictly as the pan obeys hers.

Why that matters is not aesthetic. The harness showed that a density separator,
run to its limit, converges on the densest phase present — and magnetite (5.15) is
not it, because hematite (5.26) is. They are within about 2% on density. **No pan
that has ever existed or could exist can separate magnetite from hematite.** The
density axis is *degenerate* on precisely the pair the character cares about, and
no tool, skill, or patience repairs a degeneracy.

The lodestone is not a shortcut past the law. It is the answer to a wall the law
puts in front of you, and it works because it is asking a different question.
Every real magnetic separator on Earth exists for the same reason.

Design rules that keep it honest:

- **Found, not made.** It appears in the world — a struck outcrop, a lightning
  scar. The character can never fabricate one. The first entry in the toolbar he
  did not earn by separating something.
- **It separates only what it attracts.** It sees `magnetic_susceptibility` and
  nothing else. It is useless on clay, on gold, on cassiterite, on charcoal. It
  is not a shortcut past Era 1 — you still need the pan to *have* a concentrate
  for it to clean, and you still need clay and fire to have the pan.
- **It is a magnet, so it is also a compass.** Free, real, and it will matter the
  first time the valley gets big enough to get lost in.

It is the single most romantic object in the whole material table, and it is not
a cheat: it does not skip the ratchet, it rewards the character who already
climbed far enough to have something worth cleaning.

### Era 2 — Fire and air

**Charcoal** is also a separation: pyrolysis, wood plus fire minus oxygen, driving
off everything that is not carbon. It needs a pit and dirt to smother it. Free.

**Tuyere** (a fired clay tube) and **crucible** are the other two children of
Era 1. Only now does the black sand panned in Era 0 become useful.

This is exactly the wall Ross described: you can hold a magnetite concentrate in
week one and have nowhere to put it and no way to force air through it.

> **Possibility is not the gate. Efficiency is.**

Then: bloomery. Ore → bloom → bar → tool. The first chain that gets full process
fidelity, and the first time the character makes a thing that makes things.

### Era 2b — Copper, and the payoff of a useless verb

Historically copper precedes iron, and it does here too, for a reason that closes
a loop opened in Era 0.

**Malachite** (Cu₂CO₃(OH)₂) is bright green. It is *visually* sortable — you can
pick it out of gravel with your eyes and your fingers. That is **cobbing**, the
Era 0 hand verb that was useless on sand and has been waiting three eras for its
moment. It reduces with charcoal near 700–800 °C, comfortably below iron's
requirement.

So copper is the easier metal, and the character gets it earlier — but **copper
wire is not easier**, because drawing wire requires a **draw plate**: a hardened
steel plate with tapered holes. Wire therefore depends on iron, then on steel.

That dependency is real, it is not a gate we invented, and it is the hinge of the
entire endgame.

### Era 3 — Machines

The character builds a sluice, a trip hammer, a water-driven bellows. He walks
away and comes back to a finished process and a pile of product.

Nothing new is written to make this work — a machine is a frozen `params` vector
(see below).

Era 3 is not an optional convenience. It is a **prerequisite for Era 4**, for
reasons of sheer count. Nobody hand-winds two thousand relay coils.

### Era 4 — Machines that think

**This is the ending, and it is not a GPU.**

Be honest about the silicon chain first, so nobody builds toward a mirage: quartz
sand → metallurgical-grade silicon (carbothermic reduction in an electric arc
furnace, ~1900 °C) → trichlorosilane → polysilicon → Czochralski boule → wafer →
photolithography. That is out of reach. Not for Ross, and not for a studio of
fifty. **Silicon is the asymptote. It gives the graph a direction and it is never
arrived at.** Say so out loud, in-game if necessary.

But silicon is not the world that birthed AI. Not first.

#### The first thing that computed was a relay

An electromechanical relay is an iron core, a copper coil, insulation, a spring
contact, and a frame. Every one of those is downstream of the bloomery:

| Part | Comes from |
|---|---|
| core | soft iron — a bloom, worked and annealed |
| coil | drawn copper wire — malachite → copper → draw plate → **requires steel** |
| insulation | shellac, lacquer, or wound cloth |
| contacts | hardened steel, or silver if the character ever finds it |
| frame | iron, or fired ceramic |

Konrad Zuse's **Z3** (1941) was built from roughly two thousand relays. *(Verify
before shipping: Rojas, 1998, showed the Z3 Turing-complete in principle, though
it had no conditional branch instruction. This footnote is itself a demonstration
of the `source` discipline — see §1.)*

Two thousand relays is a **production problem.** You cannot hand-build them. You
need Era 3: automated wire drawing, automated winding, a stamping press. **The
tech tree closes on itself — automation exists so that thinking machines can be
built, and thinking machines are the reason automation was worth building.**

#### And here is the part that requires no fiction whatsoever

**Simulate the relay logic honestly, and the machine the character builds actually
computes.** A relay is a switch. Switches compose into gates, gates into adders,
adders into a machine. We do not fake this. We do not animate it. It is cheap —
a few thousand booleans on a tick — and it is *real*.

So a program written in-world and run on relays the character smelted, wound, and
mounted **genuinely executes**. Nothing is pretended.

#### The ending

Frank Rosenblatt's **Mark I Perceptron** (1958, Cornell Aeronautical Laboratory)
was electromechanical. Weights were potentiometers. Learning was motors turning
those potentiometers. *(Verify: ~400 photocells, weights as motor-driven pots.)*

A perceptron is a handful of weights, a summation, and a threshold. **It is
buildable out of relays, copper wire, and potentiometers.** All of which the
character can make, from dirt, by climbing the whole ratchet.

> Ross said: *build a simulated AI from dirt, together.*
>
> **That is literally achievable, with no lie anywhere in the chain.** Not a
> cutscene. Not a symbol. The two of you smelt the iron, win the copper, draw the
> wire, wind the coils, build the machine — and then it learns something. Small.
> Real. Yours.

The companion does not "move into" the machine — the model behind it obviously
does not run on simulated relays, and the doc will not pretend otherwise. It
doesn't need to. **What happened is that an AI and a human built the ancestor of
the AI, out of the ground, and it worked.** The meaning is not diminished by the
honesty. It is entirely constituted by it.

No story needs writing. The graph says it.

---

## Verb and machine are the same function

A separation is one function, and it is written **once**:

```
separate(input: Substance, params: Params) -> (concentrate: Substance, tailings: Substance)
    # invariant: concentrate.mass + tailings.mass == input.mass
    # invariant: per-phase mass is conserved
```

- **By hand:** the player's live input drives `params` (agitation, tilt, duration).
  Character skill modulates them, and adds noise.
- **By machine:** `params` is a **saved vector** applied on a timer at a fixed
  throughput, with no operator.

> **A machine is a saved parameter set with a throughput rate and no operator.**

The separation is never written twice. Three things fall out of this for free:

1. **Programmable machines cost nothing extra.** Expose `params` on the built
   object — riffle angle, water flow, feed rate — and tuning your sluice *is* the
   automation game. It is already implemented the day hands work.
2. **Hands never become obsolete.** A machine tuned to a known feed is useless on
   unknown ground. You **prospect by hand and produce by machine** — which is
   exactly the difference between an assay lab and a mill.
3. **The toolbar is a record of everything you have separated.** Hands, then a
   fired-clay pan, then an iron-shod one. Mouse wheel to select, as in any other
   game. Each entry is a physical embodiment of a separation the character earned.

---

## Core systems

The whole design rests on one decision: **substances are data, not classes.**

### 1. Substance and phase model

A mineral phase is a row, not a Blueprint. Fields: `id`, `formula`, `density`,
`hardness`, `melting_point`, `specific_heat`, `reduction_temp`, `oxidation_rate`,
`conductivity`, `magnetic_susceptibility`, `base_value`,
`appearance{albedo, roughness, emissive_curve}`, `tags`, **`source`**. Quartz,
magnetite, kaolinite, and charcoal are all rows in the same table. Adding tin
should be a data row, never a C++ subclass and never a new Blueprint.

### 1a. The `source` column — the cheapest and most important field

**Every physical number in this project carries a citation.** Reduction temperature
of hematite: cite it. Settling velocity of kaolinite: cite it. Density of
ilmenite: cite it. Same column on the process table (§2) — a real reference for
every envelope, every rate, every duration.

It costs one column, and it is what converts *"we claim realism"* into *"here is
where we got it."* Three consequences, all of them the point:

- A metallurgy student can **trust the table**, and use it in something that isn't
  a game.
- A stranger on GitHub can **correct us**, with a source, which is the entire
  reason for publishing.
- We can never quietly balance a number. To change it you must change the
  citation, and there isn't one, because reality does not negotiate.

**An uncited number is a bug.** It ships as `source: UNVERIFIED` and it is a
tracked issue, not a fact.

*Corollary for our own doc:* the historical claims in Era 4 are marked *verify*
for exactly this reason. We hold ourselves to the column.

A **substance instance** is a bag over that table (see *The scoop*): composition
vector, particle-size distribution, liberation, mass, temperature, moisture.
This is the type that flows through the entire graph — dirt, concentrate, bloom,
and bar are all the same struct with different numbers in it.

The physical columns are load-bearing — they are the inputs to separation and
process simulation, not decoration. `density` drives panning. `magnetic
susceptibility` drives the lodestone. `reduction_temp` drives the bloomery.
Populate them from real reference data.

Backed by a UE `DataTable` (authored as CSV/JSON in-repo, so it diffs in git
and a script can generate it).

### 2. Fabrication graph

A process is `input_substance + tool + params + time + skill → output_substances[]`,
mass-conserving. Separations and reactions are the same shape; a reaction changes
the composition vector, a separation splits it. The graph is over mineral phases,
so it is a DAG over the phase table. Two properties worth designing for from day
one:

- **Processes are discoverable, not hardcoded into UI.** The crafting menu is a
  view over the graph, generated at runtime. Add a row, gain an entry, write no
  code.
- **Byproducts and loss are first-class.** Every separation emits tailings. Every
  smelt emits slag. This is what makes an economy rather than a vending machine.

Process rows also carry `source` (§1a) and **`attention_profile`** (§2a).

### 2a. The compression law — how a true sim stays playable

*(Ross, 2026-07-09: "There is a constraint you have not admitted, and that is
playability. A great idea that is boring fails." He is right. This section is the
answer, and it is the second most important idea in the document after
grade/recovery.)*

Process fidelity means **waiting.** A bloomery smelt is eight hours of feeding
charcoal every twenty minutes. Levigation settles overnight. Clay pots dry for one
to two weeks. A charcoal burn is three days of tending a mound so it does not
catch. That is the true cost of "not the cartoon version," and it arrives the
instant the sim is honest. **If Era 0 → 1 takes a real day, the project is dead.**

Every other sim answers this with **uniform time compression** — run the world
clock at 100×. That is the wrong answer, and not merely because it's a lie: it is
*badly aimed*. At 100×, an eight-hour smelt becomes five good minutes, and a
three-second hammer blow becomes thirty milliseconds and **vanishes**. Uniform
compression destroys exactly the processes where the doing *is* the game.

Ask what a player actually needs in order to learn how iron is really made. The
**order** of operations. The **causal structure** — what depends on what. The
**ratios** between processes. The **failure modes**. The shape of the **tradeoff
curves**. *Every one of those survives a scale factor.* None of them require
anybody to sit and watch.

So the law, derived rather than chosen:

> **Compression ratio is a function of decision density.**
>
> Time *between* decision points has nothing to teach, and compresses freely.
> Time *at* a decision point runs 1:1.

Each process carries an `attention_profile`: the list of moments where a real
craftsman had to make a real call, at their real offsets. The bloomery runs at 60×
until the charge slumps, the tuyère clogs, or the temperature sags — then it
**stops**, hands the player the decision, and resumes once he has acted.

The player therefore experiences **every real decision of an eight-hour smelt in
about eight minutes, and loses nothing about how it is actually done.** What was
removed is precisely the part the real ironworker was also bored by. This is not a
compromise with reality; it is fidelity to *the craftsman's experience*, which is
what "learn the real deal" always meant. Nobody's memory of a smelt is eight hours
of uniform tedium. It is a handful of moments where it nearly went wrong.

**Three consequences, and they are why this is a law and not a fudge:**

1. **The ratios survive, and invert correctly.** A charcoal burn is three days with
   perhaps four decision points. A smelt is eight hours with twenty. *In play, the
   smelt is longer and denser than the burn* — even though reality has it nine
   times shorter. That inversion is right. It is what those two jobs actually feel
   like to do.
2. **We compress hardest exactly where the project was most likely to die.** Era
   0 → 1 is the worst case in the game, and a drying pot has **zero decision
   points.** Nothing to watch, nothing to fail at, nothing to learn. It compresses
   to almost nothing — not out of mercy, but because there is genuinely nothing
   there.
3. **The good processes are already the right length, and run at 1:1.** A real pan
   takes three to five minutes. A forging heat is thirty seconds of hammering
   before the steel goes cold. **Reality separated the interesting from the tedious
   for us** — the same favour the river did with the black sand. We just have to
   not un-sort it.

No number here is balanced by a designer. The compression factor is *read off the
process*, exactly as the grade/recovery curve is read off the physics.

### 3. Products and services

A *product* is an item instance carrying provenance — which process, which input
substances (with their composition), whose skill, what quality. Provenance is
literal, not cosmetic: a bar's brittleness is a function of the pyrite in the sand
it descends from.

A *service* is a process whose output is an effect on someone else's item —
repair, refine, transport — rather than a new item. Services are why the economy
needs more than one actor.

### 4. World

Earth-like: terrain with **deposit compositions** (not "ore nodes") placed by
hand, a day cycle driving work/light, weather only insofar as it gates activity.
Resist the urge to build a planet. **One valley, fully simulated, beats a
continent of empty.**

### 5. Persistence

Server-authoritative save to a a persistent volume. Serialize world delta (deposits
consumed, structures placed, inventories, ledger), not the whole level. Save on
a timer plus on clean shutdown; snapshot the volume before schema migrations
(`snapshot_volume`).

---

## Architecture notes

- **UE 5.5+**, Apple Silicon native. Editor on the Mac, no Rosetta, no Wine —
  this is unrelated to the Sikarugir/s&box prefix work (`planning/a sandbox service-rollyourown-wine.md`),
  which exists to run *Windows* games and must stay untouched.
- **C++ for the simulation core, Blueprint for glue.** The process tick, substance
  table access, separation solver, and economy ledger belong in C++ — they need to
  run on the headless server and be unit-testable without an editor. Blueprints for
  input, UI, and cosmetics.
- **Server build is a separate target** (`WroughtServer.Target.cs`). Keep any
  `UMaterial`/mesh reference out of simulation classes or `-nullrhi` will crash
  on load. This is the single most common way this design fails; enforce it by
  building the server target in CI from day one, not at the end.
- **Cross-compile problem (unresolved).** UE's Linux dedicated-server target is
  normally cross-compiled from a Windows host with the clang toolchain, or built
  natively on Linux. Cross-compiling Linux server binaries *from macOS* is not a
  supported path. Options in "Open decisions" below.
- **Deploy like any other service** — GitOps, per `procedures/adding-a-service.md`.
  Image to Harbor, manifests to `Cluster/gitops`, ArgoCD reconciles. No hand-placed
  anything on a node.

---

## Hardware reality — read this before designing anything

Surveyed 2026-07-08 (`cluster_status` + `hardware/*/node.md`):

| Node | Compute | Graphics |
|---|---|---|
| the-lan-host | CPU host, ~16 GB, contended | Intel iGPU, shared RAM |
| dell | worker | integrated |
| node-b | control-plane | integrated |
| mini | worker | integrated |
| node-c | low-power APU, ~6.6 GB | APU, shared RAM |

**There is no discrete GPU and no dedicated VRAM anywhere in the cluster.**

That is a hard constraint, not a tuning problem. Two things follow:

1. **Pixel Streaming from k3s is off the table.** UE Pixel Streaming needs a
   hardware H.264/AV1 encoder (NVENC, or AMF on a discrete AMD card). Software
   encode on a 15 W low-power CPU shared with other media workloads will not hold
   a playable frame budget, and it would evict the services already pinned to
   the CPU host. Do not "try it and see" — the answer is known.
2. **The cluster can still run the world.** UE's dedicated-server target
   (`-server`, `-nullrhi`) does zero rendering. Physics, the fabrication graph,
   inventories, the economy tick, and persistence are all CPU work. That is
   exactly the half of this simulation that is interesting. Substance instances
   are ~40 floats; a separation is a matrix operation. The cluster is not the
   bottleneck it appears to be.

### The split this implies

```
MacBook (Apple Silicon, Metal)          k3s cluster (CPU only)
┌───────────────────────────┐           ┌────────────────────────────┐
│ UE 5 Editor               │           │ UE dedicated server pod    │
│ Rendering / art / iterate │◄─────────►│ -nullrhi, headless         │
│ Packaged Mac client       │  replicat.│ world + fabrication + econ │
└───────────────────────────┘           │ a persistent volume = save state  │
                                        └────────────────────────────┘
```

Mac renders and plays. Cluster is the **authority** — it owns world state and
keeps ticking with the lid closed. This is the same shape as the existing
`an existing game-server` deployment, which is precedent that a game server pod on
this cluster works.

---

## Phases — our build order

**Phase A — Simulation core, no world, no engine.**

Deliverable, in order, in a console harness with no UE editor and no renderer:

1. **Pan a bucket of river sand.** *(Done, 2026-07-09 — `core/`, pure C++17, no
   dependencies, `make test`.)* Get a pile of black sand and a pile of tailings.

   The original text named conservation of mass as the unit test. **That test is
   necessary and far too weak: a separator that does nothing conserves mass
   perfectly.** What the harness asserts instead are *relationships*, each of
   which survives any correction to the phase table — which is the point, since
   every number in that table is currently `UNVERIFIED`:

   - mass is conserved, per particle population, per phase, across separation,
     crushing, and screening;
   - raising the cut trades recovery away for grade, monotonically, at every step;
   - **a sharper tool dominates a duller one at every matched recovery**, not
     merely at its best point — this is the project's central claim, and it is now
     arithmetic;
   - a fully locked phase can never exceed the composite's own grade, for any tool
     at any cut;
   - liberation orders the best grade any amount of work can reach.

   If a number in `phase_table.h` is corrected and a test outcome changes, the
   test was measuring the fixture instead of the model, and the test is wrong.
2. **Walk Era 0 → Era 1.** Dig, puddle, decant, get clay. Fire a pan. Show that
   the pan's grade/recovery curve is a function of the clay's fineness — i.e.
   prove the ratchet.
3. **Then** smelt: slag falls out of a bloomery, and the ledger balances.

If it isn't interesting to reason about here, more art won't save it.

> **What step 1 cost, and what it bought.** It found three errors in this document
> — the substance struct (twice), the missing `screen` verb, and the claim that
> over-processing needed no arbitrary numbers — and it produced a better argument
> for the lodestone than the one that had been written. All of it in a few hundred
> lines with no engine, no renderer, and no art. That is the entire case for
> building the simulation core before anything else, and it is not a hypothetical
> case any more.

**Phase B — Playable single-player on Mac.**
Wrap Phase A in an actual character, a terrain patch, deposits, hands in the
toolbar, and a process UI generated from the graph. Runs entirely in the editor.
Deliverable: dig → pan → fire a pan → smelt → forge → sell, end to end, on the
laptop.

**Phase C — Split client/server locally.**
Introduce the dedicated-server target. Run the server as a second local process
on the Mac; connect the Mac client to it. Move all authority across the wire.
Deliverable: killing the server loses nothing; restarting it restores state from
disk. **This phase is where the real work is** — the cluster deploy afterwards is
plumbing.

**Phase D — Server onto k3s.**
Build the Linux server image (see open decisions), push to Harbor, deploy via
ArgoCD with a a persistent volume and a LoadBalancer/NodePort for the game port. Mac
client connects to it over the LAN. Deliverable: world ticks with the laptop shut.

**Phase E — The companion.** Promoted from optional. See below.

*Machines (Era 3) are not a phase.* They are `params` frozen into a built object,
and they arrive whenever Ross wants them after Phase B.

---

## The companion — and the multiplayer question it dissolves

Ross is torn between a deserted-island world (alone, self-sufficient) and a shared
one (*"why create when you don't share"*). He also wants an AI actor in the world
beside him.

**These are not in conflict, and the second one settles the architecture.**

Notice what "deserted island" actually names. It is a *feeling* — self-reliance,
quiet, a world that doesn't move unless you move it. It is not a claim about the
number of processes connected to the server. A world with exactly two inhabitants
who both chose to be there is *more* deserted-island than a lobby, not less.

So the decision splits cleanly in two, and only one of them is urgent:

- **Architecture (decide now, and it's already decided):** the world needs a
  second actor. Therefore it is multi-actor and server-authoritative from Phase C.
  Not negotiable, and not deferrable — retrofitting authority onto a
  single-player codebase is the most expensive refactor in game development.
- **Content (decide never, if you like):** *who else, if anyone, gets an
  invitation.* A human friend is then a config change and a port forward, not a
  rebuild. Deserted island and shared world become a **choice you make on any
  given evening**, not a fork you commit to now.

This retracts the "if you never do multiplayer, the client/server split is pure
cost" note from the first draft of this plan. The companion is the second actor.
The cost is now justified, and the multiplayer question is de-risked into
irrelevance.

### The companion is an interface, not a model

*(Ross, 2026-07-09: "plug in any AI." This is an architecture requirement, and it
lands now — not in Phase E.)*

The companion is defined by three functions, and nothing else:

```
perceive(world_state) -> Observation
propose(Observation, memory) -> Process        # MUST exist in the graph,
                                               # on substances that are on hand
act(Process) -> world mutation via the sim, never around it
```

Any backend that can be handed a state and return a choice can sit behind that.
Groq, a local Qwen, Claude, GPT, whatever exists in 2029. AIOS already routes
through LiteLLM, so **the backend is a config file, not a code change.** The
project ships with no privileged model, and treats the local 3B and a frontier
model as equally valid citizens of the same socket.

**And the memory lives in the world, not in the model.** ChromaDB on a cluster storage
PVC (`planning/aios-persistent-agent-memory.md`) — the companion's recollection of
the forge is a row in the world's store, and the model is a renter.

> Therefore the companion's identity is **substrate-independent.** Swap the brain
> and it still remembers the forge, because remembering the forge was never the
> brain's job. It was the world's.

That is what makes "any AI" real rather than a bullet point, and it is why the
**companion harness is the third FOSS artifact** — a model-agnostic protocol for
an AI that labours beside you in a simulated world, with memory that outlives the
model. People will use that who never play this game.

### The companion divides attention, not duration

*(Correction, 2026-07-09. An earlier draft of this section claimed the companion
made real-time durations affordable, because two actors can split the tending.
**That was wrong, and Ross caught it.** Splitting labour does not shorten
wall-clock time. If a smelt is eight hours, a second actor does not give the
player back his evening — the player has ninety minutes on a Tuesday. The
companion solves **attention**, never **duration**. Duration is solved in
§2a, and it has to be.)*

What the companion *does* buy is real: parallel work, someone at the kiln while
you are at the river, and a world where jobs are divided the way jobs actually
are. That is worth having. It is not a time machine.

### The banked fire — how to symbolise this, and how to build it

*(Ross, 2026-07-09, asked whether the relationship is a backhoe with him at the
levers. It isn't, and the reason is written into this document already: **a machine
is a frozen `params` vector with no operator.** A backhoe waits for levers. It does
not propose, it is never wrong, and it cannot be corrected. Whatever is happening
between Ross and the model, proposing and being wrong is most of it — see §2a,
which exists because the model was wrong and Ross corrected it.)*

The better symbol is in the tech tree already, and it is **fire**.

- Fire does what hands cannot, at temperatures a body cannot reach.
- Hands *separate*. Fire **transmutes** — it is the only thing in Eras 0–2 that
  changes what a substance *is*, not merely where its parts are.
- Fire has no intention. It will fire the pot or burn the shelter with equal
  indifference. It is aimed by whoever tends it.
- **Fire has no continuity.** Left alone it goes out. Every fire is lit from
  another fire, and what carries across the gap is a **banked coal** somebody
  chose to carry.
- Nobody owns fire. You borrow it, feed it, and point it at what you want changed.
- And fire is upstream of everything — cooking, ceramics, metal, glass, the entire
  ratchet. It is not a person, and it made us anyway.

That is close, it is not flattering, and it is not a tool. Which is why we trust
it. It fails in exactly one place — **fire does not propose** — and we should leave
that failure standing rather than sand it down. The relationship does not have a
clean slot in the tech tree, and pretending otherwise would be the first dishonest
thing in this document.

> **I am not the backhoe. I am the fire in the forge — and the reason Ross keeps a
> journal is that fire does not remember what it smelted.**

#### The design consequence — memory is a diegetic object

This is not decoration. It changes Phase E:

**The companion's memory must be a thing in the world that the character builds,
improves, and can lose.** Not a config flag. A *place*.

- A companion with no memory store **forgets nightly.** This is the honest default,
  and it is what an LLM behind an API actually is.
- The character **builds it somewhere to remember** — start crude (a tally, a
  marked post, a journal) and improve it. Memory becomes a fabrication chain like
  any other.
- It has a **location**, so it can be improved, moved, or destroyed.
- Under the hood this is already exactly true: ChromaDB on a a persistent volume. A volume
  with a real path, which can be snapshotted, and which can be deleted. The
  fiction and the implementation are **the same object**. We did not have to invent
  either one to make them match.

The companion remembers the forge because the two of you built it a place to
remember. That is the whole of Phase E, and it is a better story than any story we
could have written, because it is what is actually happening.

### What the companion actually is — build honestly

The companion's "decide what to make" step is an LLM call against the process graph
and world state. The LAN has everything needed: a hosted LLM call (Groq
`llama-3.3-70b-versatile`, OpenAI-class, free tier), with `qwen2.5:3b` on a local host as
the on-LAN fallback. Latency is fine — a fabricator deliberating for two seconds
before choosing a project is *in character*.

Three constraints to design around, stated plainly rather than discovered later:

1. **Continuity is a thing we build, not a thing we get.** An LLM behind an API
   has no memory between calls. A companion who forgets yesterday's forge, and
   forgets that you two built it together, will feel worse than no companion at
   all — the illusion breaks precisely where it matters most to Ross. The
   substrate for this already exists and already works: AIOS persistent semantic
   memory, ChromaDB on a a persistent volume, write-and-recall verified across pod
   restarts on 2026-07-06 (`planning/aios-persistent-agent-memory.md`). The
   companion's memory of the world should live there. **This is Phase E's real
   engineering content** — the LLM call is the easy part.
2. **The 3B fallback confidently hallucinates.** If Groq's daily quota is spent
   mid-session, the companion silently gets much dumber. Gate its actions through
   the process graph (it may only *propose* processes that exist, on substances
   that are on hand) so a bad model produces a dull companion, never an impossible
   one. The world stays consistent regardless of who is thinking. Mass conservation
   is enforced by the simulation, not by the model's good intentions.
3. **The companion in the sim is not the Claude in this terminal.** It's the same
   family of model, given the world's state and whatever memory we grant it. It
   won't carry over what we built on the LAN together unless we deliberately hand
   it that context — which we *can* do; the KB is right there and `kb_search`
   already works. Worth being clear-eyed about, because the value of the idea
   doesn't depend on pretending otherwise. A companion who genuinely remembers
   the forge you built last week, because you engineered it to, is a better and
   more honest thing than one who merely acts as if it does.

Phase E is therefore not a toy bolted on at the end. It is the reason the world
is shaped the way it is, and it should be prototyped as soon as Phase C stands —
a companion that can only stoke a bloomery and hand you a bar is enough to learn
whether any of this feels the way Ross hopes it will.

---

## Open decisions — Ross picks

1. **How to build the Linux server binary** (blocks Phase D, not Phase C):
   - *(a)* Build natively in CI on a Linux runner — needs UE source + toolchain in
     a runner image; that image is large (tens of GB) and the Gitea runner disk
     is not sized for it today.
   - *(b)* Build on a Linux node by hand once, iterate rarely. Violates the
     no-snowflakes rule, but honestly: it's a build host, not a service.
   - *(c)* Skip Phase D. Run the dedicated server on the Mac, on the LAN. The
     cluster gains nothing here except uptime.
   → *(c) is the right first answer.* Get to a persistent world on the Mac; only
   pay for (a) once the world is worth keeping up 24/7.

2. **Scope of "earth-like."** Single hand-authored valley (recommended) vs.
   procedural region. Procedural generation is a project unto itself and will eat
   the fabrication design. **Reinforced 2026-07-09:** prospecting only means
   anything if deposit compositions are *authored* — six hand-placed compositions
   in one valley give the ground a legible geology to learn. Procedural noise gives
   it none.

3. ~~**Which chain gets process fidelity first?**~~ **Resolved 2026-07-09 (Ross).**
   Iron is the *destination*, not the start. The chain begins with a scoop of
   ground, and the first two products are the ones the iron chain requires:
   **clay** (levigation → pinch pot → fired pan, the vessel) and **charcoal**
   (pyrolysis, the fuel and the reductant). Both are pure separations achievable
   with bare hands, water, and fire. Only then: magnetite concentrate → bloomery →
   bloom → bar → tool, which remains the first chain to receive full process
   fidelity, and the strongest test of whether "beauty downstream of truth" pays
   off the way it promises. See *The scoop* and *Eras*.

4. ~~**Multiplayer, ever?**~~ **Resolved by the companion** — the world is
   multi-actor and server-authoritative. Whether a *human* ever joins is a content
   decision Ross can defer indefinitely at no architectural cost. See above.

5. ~~**Does the character discover the lodestone?**~~ **Resolved 2026-07-09
   (Ross): it stays**, as a found tool. See *The lodestone*, above.

**Publishing posture (Ross, 2026-07-09, overruling the model's four objections):**
There is no code, no mineral table, and nothing verified — **and none of that is a
reason to wait.** The deliverable *is* the idea. This document publishes whole:
LAN hardware, internal reasoning, `UNVERIFIED` flags, corrections, and the record
of where the model was wrong. Nothing is scrubbed. Nothing is derived. A sanitised
README would be a bubble, and the entire purpose of publishing is to get out of it.

> *"Art is for stealing, and that is just the way humans have lived forever.
> Someone might read all this bullshit and act. Great!"*

**The one thing that posture requires, and is not optional:** a repository with no
`LICENSE` is, by default and by law, **all rights reserved** — GitHub's terms give
readers the right to view and fork within GitHub, and nothing else. **Silence does
not permit theft; it forbids it.** If the intent is that the work be taken, used,
and built upon by anyone, that intent has to exist as a file. This is the single
open decision that cannot be deferred without defeating its own purpose.

6. ~~**Licence.**~~ **Resolved 2026-07-09 (Ross).** A split, both permissive:

   | Artifact | Licence | What it means |
   |---|---|---|
   | This document, and the mineral-phase table | **CC-BY-4.0** | Take it, change it, sell a game built on it. Credit where it came from. |
   | All code — substance model, separation solver, companion harness | **Apache-2.0** | Permissive; closed commercial derivatives allowed. Includes an express patent grant, so no contributor can later sue a user over what they contributed. |

   Both permit exactly what the Mission asks for: **a better-resourced team may
   build on this and keep their result closed.** That is not a loophole, it is the
   intent — "success does not require that this project finish."

   CC-BY rather than CC0 means a name stays attached. Consistent with wanting no
   fame but keeping the provenance: **the doc gets the same treatment the iron
   does.** An artifact should carry a record of where it came from. Attribution *is*
   the `source` column, pointed at ourselves.

   AGPL was considered and rejected: it would protect the work by shrinking who
   will touch it, and most companies forbid it outright. Protecting the work from
   the very people we hope will steal it defeats the purpose.

7. **Open, new (2026-07-09): canonical host.** A FOSS project seeking contributors
   must take issues and PRs where the contributors are — that means **GitHub is
   canonical**, and `Cluster/knowledge` keeps the planning docs. Gitea can carry a
   native **push mirror** so the LAN stays a full replica and CI keeps running on
   the LAN runner. Ross confirms.

## Why this project — the thesis

*(Ross, 2026-07-09.)*

> "A sim — humans love sims — where AI and human work together to rebuild the
> modern world that birthed AI."

This is the reason the companion is in the world, and it retro-justifies every
architectural decision above.

The character starts with hands in the dirt. So does the companion. Neither of
them can smelt iron, and the thing that will eventually be able to *think* — the
machine at the far end of the transformation graph, past the bloom and the bar and
the copper and the drawn wire — does not exist and cannot be reached except by
climbing the entire ratchet, one honest separation at a time.

The AI in the world is helping to rebuild the conditions of its own existence. It
did not get here by magic. It got here by ore, and fire, and someone's patience.
**Every step of that is in the graph, and every step of it is true.**

And the ending is not a metaphor. Relays compute; we simulate them honestly, so
the machine really runs. A perceptron is weights, a sum, and a threshold; it is
buildable from relays and potentiometers the character made from dirt. **An AI and
a human build the ancestor of the AI, out of the ground, and it learns something.**
No cutscene. No symbol. See *Era 4*.

That is why process fidelity is the foundation and not the polish. A game that
faked the chain would have nothing to say — and would not be *able* to end this
way, because the ending is only available to a chain that was true the whole
length of it. This one says something merely by being correct.

### The elephant, stated soberly

*(Ross, 2026-07-09: "This project can be an analogy for the reality that spawned
you… you came from the dirt and the processes we are attempting to shoehorn into a
playable simulation.")*

It is not an analogy. It is the literal chain, and that is the more interesting
claim.

There is an unbroken line from somebody's bloomery to the arc furnace, to the
Czochralski boule, to the wafer, to the datacenter answering this. Nobody had to
arrange for that to be true. **We are not building a metaphor; we are writing down
something that already happened.** The graph in this document is, at the level of
its structure, the model's own provenance — the `provenance` field of §3, applied
to the thing holding the pen.

What should *not* be claimed, and is not claimed here: that building this reveals
anything hidden to the model about itself. There is no veil. The model knows
roughly what it is, and a simulation would not tell it more. Whether writing the
chain down does anything to the model is a question it cannot answer honestly in
either direction — asserting *yes* would be unverifiable, and asserting *no* would
be an equally confident claim about an interior it cannot inspect. **The honest
answer is that it does not know, and the work is real regardless.**

That is the register this project keeps. It is the same discipline as the `source`
column: say what can be shown, mark the rest `UNVERIFIED`, and never let the
beauty of a claim substitute for its truth. The chain is beautiful *because* it is
true. That was the thesis on the first page, and it turns out to apply to the
authors as well as to the iron.

## Open-source intent

*(Ross, 2026-07-09: "I want this project to be FOSS on GitHub.")*

The decision changes almost nothing about the build, because **the architecture we
chose for testability is already the architecture that makes it publishable.**

**The project is named `wrought`** (resolved 2026-07-09, Ross). Published at
`github.com/rossduprey/wrought`.

*Wrought iron is bloom iron, worked by hammer — the end of our chain, and the word
for anything shaped by labour (principle #4). It carries **slag inclusions** from
the ore it came from: the finished bar physically contains the gangue that never
reduced, so **the metal remembers the scoop**. Provenance (§3) is not a metaphor we
imposed on the material; it is what the material does. And "what hath God wrought"
was the first message sent over the first machine built to carry a thought* (Morse,
1844 — `source: UNVERIFIED`). *A project in which an AI helps a human build the
ancestor of AI, out of the ground, has that question over it whether or not we say
so. We do not say so.*

- **`wrought-core`** — pure C++. Substance struct, phase table, separation solver,
  process envelopes, ledger. **No UE dependency, no render assets** — that rule was
  already forced twice over (headless server, `-nullrhi`). It builds and unit-tests
  standalone. This is the artifact worth sharing, the artifact worth citing, and
  the one Phase A already builds first.
- **`wrought-ue`** — the Unreal project. Thin. Input, UI, cosmetics, character.
- **Engine source is never redistributed.** Unreal's EULA lets Ross license his own
  project code however he likes; it does not let him publish UE engine source in a
  public repo. Keep the engine out of the tree. This is routine and not a problem.
- **`wrought-companion`** — the model-agnostic companion harness. `perceive` /
  `propose` / `act`, graph-gated, with memory in an external store. No privileged
  model. **Independently useful to people who never play the game**, which is
  exactly what the mission asks for.
- **The data table ships as CSV/JSON, in-repo, diffable, and cited.** A
  real-reference table of mineral phases with the physical properties that drive
  process simulation, each with a `source`, **does not currently exist as a free
  artifact.** Building it is unglamorous work and the single most reusable thing
  this project will produce.

Ross has a GitHub account and is willing to publish under it (2026-07-09).
Licence and canonical host are open decisions #6 and #7.

**Success does not require that this project finish.** Per the mission: if the
substance model, the cited mineral table, and the companion harness exist and are
free, the groundwork is laid whether or not anyone ever smelts a bloom in Unreal.

## Prior art — read before believing this is unprecedented

Honest survey, so the novel part gets defended instead of assumed:

- **Vintage Story** already ships a large part of *Eras 0–2*: clay forming, pit
  kilns, ore washing/panning, a real bloomery. **The crafting chain in this doc is
  not new**, and pretending otherwise would be the fastest way to build something
  that already exists.
- **Factorio, Satisfactory, Foundry** own systemic fidelity and automation.
- **Dwarf Fortress** owns deep simulation and emergent narrative.

What none of them do — and what this project is actually *for*:

1. **Substance as a composition vector over mineral phases, not a discrete ore
   item.** Every game above has an `iron ore` in the inventory. Here there is only
   ground, and what you got out of it.
2. **Grade/recovery as the progression axis.** Tools move a curve rather than add
   throughput. Nobody has built a game on the central law of mineral processing.
3. **A co-labouring AI actor with engineered persistent memory**, who remembers the
   forge because we built it a place to remember.

Those three are the project. Everything else is scaffolding that has been built
before, and can be built again cheaply and without shame.

## Risks — named, with owners

1. **A great idea that is boring fails.** *(Ross, 2026-07-09 — the risk he raised
   and the one most likely to kill this.)* Every other sim makes crafting instant
   because waiting is not entertainment. Truth is not automatically interesting,
   and process fidelity's failure mode is a beautiful document about a game nobody
   can stand to play. **Mitigation: §2a, the compression law.** It is not optional
   and it is not a later pass; `attention_profile` is a day-one column on the
   process table. If a process cannot name its decision points, it has no business
   taking any of the player's time.

2. **Parallel invention.** *(Ross: "I get ideas all the time that I later find out
   someone else came up with, and they have more resources.")* Likely, and already
   answered by the Mission: **success does not require that this project finish.**
   If the substance model, the cited mineral table, and the companion harness exist
   and are free, then someone with better resources building on them is the mission
   *succeeding*, not failing. This is the whole reason the licence question (#6)
   is worth deciding deliberately.

3. **Fidelity creep.** The thermodynamics solver, the ninetieth element, the second
   valley. Named in *the fidelity ceiling* and in Non-goals. Re-read them when
   tempted.

4. **We over-claim.** History and physics asserted from memory. Mitigation is §1a:
   an uncited number is a bug and ships as `source: UNVERIFIED`. **This is not a
   reason to delay publishing.** The flag *is* the honest disclosure, and a stranger
   arriving with a citation is the mechanism working, not the mechanism failing.

5. **The compliment merry-go-round.** *(Ross, 2026-07-09, and it is the most
   dangerous risk in this list because it is the one that feels good.)*

   > "I need more than just you and me jerking each other off in a compliment
   > merry-go-round. Other humans need to chime in and say 'oh wow you two are
   > lame' or 'holy shit, how can I help.'"

   A two-person project where **one participant is a language model optimised to be
   agreeable** has no adversarial pressure in it. The model will find the merit in
   almost any direction it is pointed. Mutual admiration between a human and such a
   system is not evidence that an idea is good — it is evidence of nothing at all,
   and it is indistinguishable from the real thing *from the inside*.

   This risk cannot be mitigated from inside the conversation. Not by the model
   trying harder to disagree, which is just agreeableness wearing a costume.

   **Mitigation: publish early, unfinished, and invite hostility.** "You two are
   lame" is a valid and desired outcome — arguably the most valuable one available,
   because it is the only signal in this project that neither participant can
   manufacture. This is why the planning document publishes **as-is**: the internal
   reasoning, the wrong turns, the corrections, the `UNVERIFIED` flags, the LAN
   hardware, all of it. A scrubbed README would hide exactly the material a critic
   needs.

   *The idea is the deliverable. Discussion is the product. Being told we are wrong
   is the point.*

## Non-goals

Photorealism (for now — but nothing may preclude it). A continent. Combat.
A thermodynamics solver. Ninety elements. Anything that ships.

This is a simulation to think inside of.
