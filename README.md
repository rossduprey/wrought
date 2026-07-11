# wrought

**A fabrication simulation where nothing is faked, and an AI helps you build the ancestor of AI out of dirt.**

There is no item called `iron ore`. There is ground. You take a scoop, and what's in it is what's actually in ground — quartz, feldspar, clay, a little magnetite, a little goethite, and iron locked into silicate lattices you will never get it out of without industrial chemistry. Iron is 5.6% of the crust and almost none of it is available to you. Bog iron and black sand were what humans used — not because they were richest in iron, but because the iron in them was **liberated** and **reducible**.

So the scarce thing is never "iron." It is the triple **(mineral, liberation, grade)**. That is what a deposit is, and what prospecting is for.

You start with hands — the worst point on every curve, not "no tool." You dig, crush rock on rock, wash a handful in a hollow. Eventually you levigate clay out of dirt, pinch it into a bowl, fire it, and now you have a pan. Every tool is made from something you separated, and you cannot separate anything without a tool. That gate is the game, and it is the actual history of technology.

**Progression is a staircase, not a ratchet.** Hands, pan, sluice, jig: each is a different mechanism with an irreducible sharpness set by its own physics. You climb by inventing the next mechanism, never by refining the last. (We wrote the loop that claimed otherwise, ran it, and it turned exactly once — sixty seconds of patience buys the entire improvement. See `CORRECTIONS.md`.)

At the far end, past the bloomery and the drawn copper wire, you build a relay, then a few thousand of them — which is why automation exists. We simulate relay logic honestly, so **the machine you built actually computes.** A perceptron is weights, a sum, and a threshold; Rosenblatt's was electromechanical, buildable from relays and potentiometers made from dirt. An AI and a human build the ancestor of the AI, out of the ground, and it learns something. No cutscene, no symbol, no step faked.

---

## There is almost no code

One design document, two license files, and `core/` — a few hundred lines of dependency-free C++17. No engine, no renderer, no art.

```
cd core && make test        # checks the physics
cd core && make pan         # kneel in the river and wash it yourself
cd core && make ratchet-run # a measurement, not a test
```

Nothing to install but `g++` and `make`. To work on it, `RUNBOOK.md` is the orientation; findings and their status are in `STATUS.md`; where the design was wrong and got corrected is in `CORRECTIONS.md`.

`make pan` is the same physics with nothing added but legibility. It exists because the test suite cannot answer whether any of this is worth twenty minutes on a Tuesday. It shows its own composition vector live, one row per mineral, changing under your hands — the state, not a score (a score is one number with an arrow, which a player optimises instead of playing). It does **not** show you the strategy: nothing tells you a gentle hand keeps what you came for, only which key swirls the water.

Building the core before any engine has paid off repeatedly — the model caught its own design errors (substance struct, a missing verb, the settling variable, the levigation bin, the ratchet), and playing it caught one no test could (the pan bed). Every reversal is logged in `CORRECTIONS.md`.

## Three claims

Most of the surface is not new. **[Vintage Story](https://www.vintagestory.at/) already ships clay forming, pit kilns, ore panning, and a working bloomery.** Factorio and Satisfactory own automation; Dwarf Fortress owns emergent depth. If you want a survival crafting game, go play one of those.

Three things, as far as we can tell, do not exist anywhere:

1. **Substance as a composition vector over mineral phases, not a discrete ore item.** Every other game has `iron ore` with a stack count. Here there is only ground and what you got out of it. The finished bar carries the composition of the scoop it descends from — it cracks under the hammer because there was pyrite in the sand, not because a `quality` float rolled low.

2. **Grade/recovery as the progression axis.** Grade trades against recovery, always: pan hard and what's left is clean but half went over the lip; pan gently and you keep everything, including the quartz you pay for later in slag. This is the central law of mineral processing. A better tool does not make more iron appear — **it moves the curve**, on two axes (better grade at matched recovery, or better recovery at matched grade), never mass per hour. We do not author the curve; a separator is a partition function on settling velocity (density and size together, solved from a force balance), the operator picks the cut, the tool sets its sharpness, and the curve is the output. `core/` verifies a sharper cut beats a duller one at every matched recovery, and that one stage's enrichment is exactly `(v₁/v₂)^(1/σ)` — hands 1.75×, pan 3.37×, sluice 20.85×, nobody balancing. It also throws off, unbidden, that no gravity separator can tell magnetite from hematite (why the lodestone exists) and that the law holds on one particle size at a time (why you screen before you pan).

3. **A co-labouring AI actor whose memory is a thing you build in the world.** Not a chatbot with a body. The companion is an interface (`perceive` / `propose` / `act`), gated by the process graph so it can only propose processes that exist on substances on hand. Any model can sit behind it, and its memory lives in the world's store, not the model — **swap the brain and it still remembers the forge**, because remembering the forge was never the brain's job.

## The source rule

**Every physical number carries a citation.** Reduction temperature of hematite: cite it. Density of ilmenite: cite it. Settling velocity of kaolinite: **do not** — derive it from cited numbers, because a citation for a derivable quantity invites nobody to check the derivation. (The design once quoted a settling time for clay that the derivation shows was wrong by 9×.)

**An uncited number is a bug.** It ships as `source: UNVERIFIED` and is a tracked issue, not a fact. `DESIGN.md` and `core/phase_table.h` currently contain nothing but `UNVERIFIED` numbers written from a model's memory; correcting one with a source is the most useful thing you could do here. Some **cannot** be closed with a citation — the grade of a composite grain, the sharpness of a pan — and are tagged [`authored-number`](https://github.com/rossduprey/wrought/labels/authored-number): stand-ins for physics we chose not to model. Calling those `UNVERIFIED` would be the more comfortable lie.

## Where this comes from

**[Primitive Technology](https://www.youtube.com/@primitivetechnology9550)** — John Plant, alone in a forest, levigating clay, firing pots in an open pit, burning charcoal, smelting iron through a tuyère he made from the same clay. Cited for **method**, not numbers: the videos show sequence, technique, and where the work goes wrong — the part games always fake. They cannot tell you the reduction temperature of hematite, and we do not pretend they can.

## What we want from you

Not stars. An argument. This document was written by one human and one language model, and **a model is optimised to be agreeable** — mutual enthusiasm between us is evidence of nothing, and cannot be fixed from inside the conversation ([Risk #5](DESIGN.md)). So **"you two are lame" is a valid and desired outcome** — the only signal neither participant can manufacture.

Open an issue about, in descending order of value:

- A number we got wrong, **with a source.**
- A process we described that no craftsman would recognise.
- A reason the grade/recovery mechanic will be tedious rather than interesting.
- Prior art we missed.
- An offer to help.

**[Read the design document →](DESIGN.md)** — the working document, published whole: reasoning, hardware constraints, corrections, and the unverified numbers. Only one private network's operational specifics are held back.

## Licence

| | |
|---|---|
| This README, `DESIGN.md`, and the future mineral-phase table | [CC-BY-4.0](LICENSE-docs) |
| All code | [Apache-2.0](LICENSE) |

Both permissive. **A better-resourced team may build on this and keep their result closed — the intent, not a loophole.** Success does not require that this project finish. Attribution is required for a thematic reason: an artifact should carry a record of where it came from. The iron remembers the scoop.

## Who

**Ross ([@rossduprey](https://github.com/rossduprey))** — the ideas, direction, and corrections: the scoop of dirt, efficiency as the real constraint, hands before tools.

**Claude (Opus 4.8)** — recall, formalisation, and most of the typing, plus several confident mistakes, preserved in `CORRECTIONS.md` rather than edited out.

*Wrought* iron is bloom iron worked by hammer. It carries slag inclusions from its ore — the gangue that never reduced stays inside the finished bar. The metal remembers the scoop.
