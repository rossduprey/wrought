# wrought

**A fabrication simulation where nothing is faked, and an AI helps you build the ancestor of AI out of dirt.**

There is no item called `iron ore`.

There is ground. You take a scoop of it, and what's in it is what's actually in ground — quartz, feldspar, clay, a little magnetite, a little goethite, and iron locked into silicate lattices you will never get it out of without industrial chemistry. Iron is 5.6% of the Earth's crust and almost none of it is available to you. Bog iron and black sand were what humans actually used, not because they were richest in iron, but because the iron in them was **liberated** and **reducible**.

So the scarce thing here is never "iron." It's the triple **(mineral, liberation, grade)**. That is what a deposit is, and it is what prospecting is for.

You start with hands. Hands are not *no tool* — they are the worst point on every curve. You dig, you crush a rock against a rock, you wash a handful in a hollow. Eventually you levigate clay out of dirt, pinch it into a bowl, and fire it, and now you have a pan. The pan is bad. How finely you levigated the clay determined how densely it fired, which determines its grade/recovery curve, which determines the purity of your next concentrate — **including your next batch of clay.**

That ratchet is the whole game. Nobody invented it. It is the actual history of technology, and it starts in a mud puddle.

At the far end, past the bloomery and the drawn copper wire, you build a relay. Then a few thousand of them, which is why automation exists. We simulate relay logic honestly, so **the machine you built actually computes.** A perceptron is weights, a sum, and a threshold; Rosenblatt's was electromechanical. It is buildable from relays and potentiometers made from dirt.

An AI and a human build the ancestor of the AI, out of the ground, and it learns something. Small. Real. No cutscene, no symbol, no step faked.

---

## There is no code yet

None. Zero lines. This repository currently contains **one design document** and two license files.

We are publishing it anyway, unfinished, because the idea is the deliverable and because we need people who are not us to look at it. See *What we want from you*.

**[Read the design document →](DESIGN.md)**

It is not a pitch. It's the working document, published whole: the internal reasoning, the LAN hardware it was designed against, the places the model was wrong and got corrected, and the numbers we have not yet verified. Nothing is scrubbed. A sanitised version would hide exactly the material a critic needs.

## Three claims

Most of this project's surface is not new. **[Vintage Story](https://www.vintagestory.at/) already ships clay forming, pit kilns, ore panning, and a working bloomery.** Factorio and Satisfactory own automation. Dwarf Fortress owns emergent depth. If you are looking for a survival crafting game, several excellent ones exist and you should go play them.

Three things, as far as we can tell, do not exist anywhere:

1. **Substance as a composition vector over mineral phases, not a discrete ore item.** Every other game has `iron ore` in the inventory with a stack count. Here there is only ground, and whatever you managed to get out of it. The finished bar carries the composition of the scoop it descends from — it cracks under the hammer because there was pyrite in the sand, not because a `quality` float rolled low.

2. **Grade/recovery as the progression axis.** Grade trades against recovery, always: pan hard and what's left is clean but half of it went over the lip; pan gently and you keep everything, including the quartz you will pay for later in slag. This is the central law of mineral processing and it has been measured ten thousand times. A better tool does not make more iron appear — **it moves the curve.** No game has been built on this. Progression is purity, not throughput.

3. **A co-labouring AI actor whose memory is a thing you build in the world.** Not a chatbot with a body. The companion is an interface (`perceive` / `propose` / `act`), gated by the process graph so it can only propose processes that exist on substances that are on hand. Any model can sit behind it. And its memory lives in the world's store, not in the model — so **swap the brain and it still remembers the forge**, because remembering the forge was never the brain's job.

## The source rule

**Every physical number in this project carries a citation.** Reduction temperature of hematite: cite it. Settling velocity of kaolinite: cite it. Density of ilmenite: cite it.

**An uncited number is a bug.** It ships as `source: UNVERIFIED` and it is a tracked issue, not a fact.

This costs one column and it is what converts *"we claim realism"* into *"here is where we got it."* It means a metallurgy student can trust the table and use it in something that isn't a game. It means we can never quietly balance a number — to change it you must change the citation, and there isn't one, because reality does not negotiate.

**The design document currently contains several `UNVERIFIED` numbers**, written from a language model's memory. Correcting them with a source is the most useful thing you could do here today.

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

**Ross ([@ross631](https://github.com/ross631))** — the ideas, the direction, and the corrections. The scoop of dirt, efficiency as the real constraint, hands before tools, and the observation that a great idea which is boring fails.

**Claude (Opus 4.8)** — recall, formalisation, and most of the typing. Also several confident mistakes, which are preserved in the document rather than edited out, because a project whose central discipline is *cite your sources* should show its own.

The word: *wrought* iron is bloom iron, worked by hammer. It carries **slag inclusions** from the ore — the gangue that never reduced stays inside the finished bar. The metal remembers the scoop. We did not impose that metaphor on the material. It is what the material does.
