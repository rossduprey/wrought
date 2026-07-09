# wrought

**A fabrication simulation where nothing is faked, and an AI helps you build the ancestor of AI out of dirt.**

There is no item called `iron ore`.

There is ground. You take a scoop of it, and what's in it is what's actually in ground — quartz, feldspar, clay, a little magnetite, a little goethite, and iron locked into silicate lattices you will never get it out of without industrial chemistry. Iron is 5.6% of the Earth's crust and almost none of it is available to you. Bog iron and black sand were what humans actually used, not because they were richest in iron, but because the iron in them was **liberated** and **reducible**.

So the scarce thing here is never "iron." It's the triple **(mineral, liberation, grade)**. That is what a deposit is, and it is what prospecting is for.

You start with hands. Hands are not *no tool* — they are the worst point on every curve. You dig, you crush a rock against a rock, you wash a handful in a hollow. Eventually you levigate clay out of dirt, pinch it into a bowl, and fire it, and now you have a pan.

Every tool you will ever hold is made out of something you separated, and you cannot separate anything without a tool. That gate is the whole game. Nobody invented it — it is the actual history of technology, and it starts in a mud puddle.

What the gate is *not* is a spiral. This document used to claim that a finer levigation fires a denser pan, whose better curve yields a purer concentrate, and therefore a finer clay, and round again forever. We wrote the loop, ran it, and **it turns exactly once.** The pan's sharpness is identical at generation one and generation five, to four decimal places, because the only thing coarse enough to blur a pan is sand — and sand is the first thing to fall out of standing water. Sixty seconds of patience buys the entire improvement. The next forty-five hours buy one part in ten thousand.

So progression is a **staircase, not a ratchet.** Hands, pan, sluice, jig: each is a different *mechanism*, and each has an irreducible sharpness set by its own physics — the wrist, the steadiness of the flow, the pulse. You climb it by inventing the next mechanism, never by refining the last one. That is also the actual history of technology. Nobody perfected the pan. They built a sluice.

And it turns out the first step of the staircase runs *downhill*. A pot pinched from dirt you did not levigate is a **worse** separator than your bare hands — 1.44× enrichment against 1.75×. The clay, not the fire, is what makes pottery worth inventing. We did not design that. We found it.

At the far end, past the bloomery and the drawn copper wire, you build a relay. Then a few thousand of them, which is why automation exists. We simulate relay logic honestly, so **the machine you built actually computes.** A perceptron is weights, a sum, and a threshold; Rosenblatt's was electromechanical. It is buildable from relays and potentiometers made from dirt.

An AI and a human build the ancestor of the AI, out of the ground, and it learns something. Small. Real. No cutscene, no symbol, no step faked.

---

## There is almost no code

There is one design document, two license files, and `core/` — a few hundred lines of dependency-free C++17 that pans a bucket of river sand. No engine, no renderer, no art.

```
cd core && make test        # 52 assertions about the physics
cd core && make pan         # kneel in the river and wash it yourself
cd core && make ratchet-run # watch the design document lose an argument
```

Nothing to install: `g++` and `make`. If you are about to work on it rather than read it, `RUNBOOK.md` is the orientation — the toolchain, the map of `core/`, and which of the findings below are still provisional.

`make pan` is the same physics with nothing added but *legibility*. It exists because the one question the test suite cannot answer by passing is whether any of this is worth doing for twenty minutes on a Tuesday.

The pan shows you its own composition vector, live, one row per mineral, changing under your hands as you wash. That was not the original design — the original design showed you a picture of a pan and no numbers, on the theory that there is no assay laboratory in a river. But a *score* and a *state* are not the same thing, and only the first one is a lie. A player given one number with an arrow pointing up will optimise it instead of playing; a player given the composition of what is in front of him is being given the thing this project claims is its central novelty, and hiding it made the novelty invisible. The panner has no assay. **The panner is not alone** — claim #3 below is a co-labouring AI, and asking it what is in the dirt is not cheating, it is the relationship. Grade and recovery are always shown together, because either one alone is a score again.

What it does *not* show you is the strategy. Nothing in the game tells you that a gentle hand keeps what you came for. It only tells you which key swirls the water.

It exists to answer one question: does the data model survive contact? It did not. **On its first run it found three errors in the design document** — the substance struct (twice), a missing verb, and a claim that a tradeoff was free when it has to be bought — and it produced a better argument for one of the design's own set pieces than the one that had been written.

Then we acted on one of our own open issues — a suspicion that the model separated on the wrong variable — **and it found four more.** Two tests had been passing for the wrong reason. The grade/recovery law turned out to hold only within a single particle-size class. Stokes' law, which the design proposed to derive its settling numbers from, is wrong by a factor of 136 on gravel. And levigation's "clay stays suspended for an hour" is not a fact about clay; it is a fact about how deep you dug the hole.

Then a human played it for twenty minutes, and **it found one more, which no test could have.** Watching the live composition drain away, he could see that a pan washed for a minute keeps 3% of its black sand. That is false; a real panner does not throw away 97% of his gold. The model had every particle in the pan seeing the same moving water. Real grains stratify — they settle at their own terminal velocity while the swirl stirs them back up, and the equilibrium between those two is the same exponential profile that sets the density of the atmosphere with height. Only the top skin of the pan is in the water. Everything under it is a bed, and a bed cannot be washed away.

The fix added **no new state**. The fraction of a grain population in the exposed skin is `exp(-v/v_mix)`, a function of settling velocity and nothing else; and `v_mix`, the mixing scale, is the shear velocity, which is the cut — the number already on the screen. The pan needs no vertical dimension. What fell out of it, unasked: while the quartz lies on top of the magnetite the magnetite is buried and cannot leave the pan at any cut, so a gentle hand beats a hard one at *every* matched recovery. Patience became a strategy the moment the bed existed, and it was not a strategy the day before. Nobody put it there.

Then we built the ratchet in order to prove it, **and it broke the design's central claim.** Two defects had to be fixed before the question could even be asked. The model could not levigate at all: its finest size bin ran from 3.9 µm to 62.5 µm and stood for "clay and silt," which are precisely the two things levigation separates. Its representative diameter, 15.6 µm, *is silt*. The design document had published a settling table — clay takes 9.1 hours to fall through 0.10 m — that `core/` could not reproduce, and drained the whole bin out of that column in 520 seconds instead. Nothing noticed until something tried to use it.

And levigation, once written, authors nothing. A batch decant's partition function can be derived exactly: fill to depth `h`, stir, wait `t`, and a grain falling at `v` has dropped `v·t`, so what you pour off is `(clear − v·t)/h`. It is a ramp in velocity rather than a sigmoid, it terminates, and its imperfection is **exactly 3.0** for every vessel, every charge, and every wait. There is no sharpness to invent. The second separator in the project has no free parameters at all.

With that in hand the loop ran, and did not turn. The clay grade pins at 0.843 in every vessel at every wait, because it was never a property of the tool — it is the composition of the deposit's clay-sized fraction, and clay-sized quartz and clay-sized kaolinite fall at speeds differing by **1.031×**. That is the same 1.03× that makes magnetite indistinguishable from hematite and put the lodestone in Era 0. So Era 1 has a lodestone-shaped hole, and the tool that fills it is real: **deflocculation**, which separates on surface chemistry instead of settling velocity, which every potter on earth does with a handful of wood ash, and which we do not model.

Those corrections are now in the document, dated, next to the sentences they replaced. Including the one on the front page of this file.

We are publishing all of it unfinished, because the idea is the deliverable and because we need people who are not us to look at it. See *What we want from you*.

**[Read the design document →](DESIGN.md)**

It is not a pitch. It's the working document, published whole: the internal reasoning, the LAN hardware it was designed against, the places the model was wrong and got corrected, and the numbers we have not yet verified. Nothing is scrubbed. A sanitised version would hide exactly the material a critic needs.

## Where this comes from

**[Primitive Technology](https://www.youtube.com/@primitivetechnology9550)** — John Plant, alone in a forest, levigating clay out of dirt, firing it in an open pit, burning charcoal in a mound, and smelting iron through a tuyère he made from the same clay. No narration. Nothing skipped. If you want to know what this project is trying to be a simulation *of*, watch an hour of that channel and skip the rest of this document.

He is cited for **method**, not for numbers. The videos show sequence, technique, and where the work actually goes wrong — which is the hard part and the part games always fake. They cannot tell you the reduction temperature of hematite, and we do not pretend they can. Those are two different kinds of evidence and we keep them apart.

## Three claims

Most of this project's surface is not new. **[Vintage Story](https://www.vintagestory.at/) already ships clay forming, pit kilns, ore panning, and a working bloomery.** Factorio and Satisfactory own automation. Dwarf Fortress owns emergent depth. If you are looking for a survival crafting game, several excellent ones exist and you should go play them.

Three things, as far as we can tell, do not exist anywhere:

1. **Substance as a composition vector over mineral phases, not a discrete ore item.** Every other game has `iron ore` in the inventory with a stack count. Here there is only ground, and whatever you managed to get out of it. The finished bar carries the composition of the scoop it descends from — it cracks under the hammer because there was pyrite in the sand, not because a `quality` float rolled low.

2. **Grade/recovery as the progression axis.** Grade trades against recovery, always: pan hard and what's left is clean but half of it went over the lip; pan gently and you keep everything, including the quartz you will pay for later in slag. This is the central law of mineral processing and it has been measured ten thousand times. A better tool does not make more iron appear — **it moves the curve.** No game has been built on this.

   Progression is **the curve**, which has two axes: better grade at a matched recovery, or better recovery at a matched grade. It is never mass per hour. *(This sentence used to read "progression is purity, not throughput," and step 2 caught it. A fired pot returns 68% of the clay in a charge where a scraped hollow returns 4.8%, at an identical grade of 0.843 — that is the recovery axis moving outward, and for an hour we mistook it for throughput and thought we had contradicted ourselves. We had not. **Throughput is a bigger shovel:** more dirt per hour, same curve, no progression. A bigger pot digs no more dirt. It loses less of the clay in the dirt it already has.)*

   We do not author that curve, because a curve you author is a curve you balance. A separator is a partition function: the probability a particle reports to the concentrate, given the speed at which it falls through water — which is its density and its size, together, solved from a force balance rather than looked up. The operator picks the cut; the tool sets how sharp it is. The curve is the *output*.

   `core/` verifies that a sharper cut beats a duller one at **every** matched recovery, not merely at its best point. Better than that, it shows the enrichment factor of a single stage is exactly `(v₁/v₂)^(1/σ)` — so "a better tool moves the curve" is a closed-form expression rather than a promise. Cupped hands enrich magnetite over quartz by 1.75× per pass; a fired pan, 3.37×; a sluice, 20.85×. Nobody balanced those.

   It also throws off, unbidden, two things nobody wrote: that no gravity separator can ever tell magnetite from hematite, which is why the lodestone is in the game — and that the law itself only holds on one particle size at a time, which is why you wash the mud off and pick the pebbles out before you pan. An unscreened pan, run two hundred and fifty-six times, converges on a handful of gravel.

3. **A co-labouring AI actor whose memory is a thing you build in the world.** Not a chatbot with a body. The companion is an interface (`perceive` / `propose` / `act`), gated by the process graph so it can only propose processes that exist on substances that are on hand. Any model can sit behind it. And its memory lives in the world's store, not in the model — so **swap the brain and it still remembers the forge**, because remembering the forge was never the brain's job.

## The source rule

**Every physical number in this project carries a citation.** Reduction temperature of hematite: cite it. Density of ilmenite: cite it. Viscosity of water: cite it.

Settling velocity of kaolinite: **do not cite it.** Derive it, from numbers that are cited, because a citation for a derivable quantity invites nobody to check the derivation. That distinction is not pedantry — the design originally quoted a settling time for clay, and the derivation says it was wrong by a factor of nine.

**An uncited number is a bug.** It ships as `source: UNVERIFIED` and it is a tracked issue, not a fact.

This costs one column and it is what converts *"we claim realism"* into *"here is where we got it."* It means a metallurgy student can trust the table and use it in something that isn't a game. It means we can never quietly balance a number — to change it you must change the citation, and there isn't one, because reality does not negotiate.

**The design document and `core/phase_table.h` currently contain nothing but `UNVERIFIED` numbers**, written from a language model's memory. Correcting them with a source is the most useful thing you could do here today, and [they are all filed as issues](https://github.com/rossduprey/wrought/issues).

Some of them **cannot** be closed with a citation, and those are tagged [`authored-number`](https://github.com/rossduprey/wrought/labels/authored-number) rather than [`unverified-number`](https://github.com/rossduprey/wrought/labels/unverified-number). They are numbers we invented — the grade of a composite grain, the efficiency of a screen, the sharpness of a pan. No reference exists to look them up in, because they are stand-ins for physics we chose not to model. Calling those `UNVERIFIED` would imply somebody could go and check, and that would be the more comfortable lie.

## What we want from you

Not stars. Discussion, and preferably an argument.

This document was written by one human and one language model, over a long evening. **A language model is optimised to be agreeable.** It will find the merit in nearly any direction it is pointed, which means mutual enthusiasm between the two of us is not evidence that the idea is good — it is evidence of nothing at all, and from the inside it is indistinguishable from the real thing. That risk cannot be fixed from inside the conversation. It is [Risk #5 in the document](DESIGN.md), and it is the one that feels good while it kills you.

So: **"you two are lame" is a valid and desired outcome.** It is the only signal in this project that neither participant can manufacture.

Useful things to open an issue about, in descending order of value:

- A number we got wrong, **with a source.**
- A process we described that no craftsman would recognise.
- A reason the grade/recovery mechanic will be tedious rather than interesting.
- Prior art we missed. We would rather learn it exists than build it twice.
- An offer to help.

## Licence

| | |
|---|---|
| This README, `DESIGN.md`, and the future mineral-phase table | [CC-BY-4.0](LICENSE-docs) |
| All code | [Apache-2.0](LICENSE) |

Both are permissive. **A better-resourced team may build on this and keep their result closed, and that is the intent, not a loophole.** Success does not require that this project finish. If the substance model, the cited mineral table, and the companion harness exist and are free, the groundwork is laid whether or not anyone ever smelts a bloom in Unreal.

Attribution is required, and the reason is thematic rather than vain: an artifact should carry a record of where it came from. The iron remembers the scoop. Attribution is the `source` column, pointed at ourselves.

## Who

**Ross ([@rossduprey](https://github.com/rossduprey))** — the ideas, the direction, and the corrections. The scoop of dirt, efficiency as the real constraint, hands before tools, and the observation that a great idea which is boring fails.

**Claude (Opus 4.8)** — recall, formalisation, and most of the typing. Also several confident mistakes, which are preserved in the document rather than edited out, because a project whose central discipline is *cite your sources* should show its own.

The word: *wrought* iron is bloom iron, worked by hammer. It carries **slag inclusions** from the ore — the gangue that never reduced stays inside the finished bar. The metal remembers the scoop. We did not impose that metaphor on the material. It is what the material does.
