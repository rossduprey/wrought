# Status

What is built and what is open. Updated per session. Why the sim is shaped this way → `DESIGN.md`; reversals → `CORRECTIONS.md`; how to work on it → `RUNBOOK.md`.

## Built

- **Core (`core/`, header-only):** phase table, substance, settling, separation (Tromp), magnetic, smelt (bloomery + copper/tin furnace + roast), forge (consolidate + shape + cast + alloy), levigate (+ flocculate), fire, geology (spatial field), fuel (wood → pyrolysis → charcoal; the two-gate finding; fire tiers), char (the charcoal-pit transform; the seal is the gate).
- **Chain:** dig → carry → break → pan → furnace, end to end. Three metals (iron, copper, tin) + one alloy (bronze); both faces of the sulfur problem (red-short in iron, roast in copper).
- **Playable slices (9 TUs):** pan, dig, breaker, furnace, carry, char, gather. `ratchet` is an instrument, not a slice.

## Settled findings

One line each; full statements in `DESIGN.md`, reversals in `CORRECTIONS.md`.

- Progression is a staircase, not a ratchet.
- Levigation authors nothing (imperfection = 3.0 exactly).
- Levigation divides the clay bin (6.977×) — no lodestone-shaped hole.
- Throughput is a bigger shovel; a bigger pot is progression.
- Fluxless bloomery smelts only at Fe/SiO₂ ≥ 1.859 — the pan alone cannot make smeltable ore.
- A wrought bar's floor is connectivity (~1.5%), not effort.
- Form is free at any heat; hardness is only cold.
- The contaminant no separator sees (sulfur) breaks the tool (red-short).
- Abundance is not the gate; melting point is (copper pours, iron sponges).
- Alloying is not averaging — bronze lands off the end of both parents.
- Sulfur: cured at the furnace (copper roast) or hidden to the anvil (iron).
- A stony floor swallows the feed (shelter); it does not blur it.
- Co-location is the gate, now geometry.
- `roast()` cures only the sulfide the crushing freed — the recovery ceiling is set at the breaker.
- Charcoal is the reductant, not merely a hotter fuel — a wood fire clears the reduction onset and still smelts nothing.
- A fire has two independent gates: reductant (carbon in the charge, chemical) and peak temperature (fuel × air, thermal).
- A charcoal pit works by suffocating its own fire — open it to the air and it burns the char to ash; the seal, not the wood, sets the yield.
- Gathering by hand wins tinder and sticks, never timber — the bulk fuel and every haft are locked behind the axe (hands are tool #1, the worst point on the curve).
- The reductant a copper smelt needs is tiny — a small pit of hand-gathered sticks covers it many times over; charcoal binds the fire's heat (and the bloom's blast), not the copper reduction. The furnace burns made charcoal now, not a free bed.

## Open

- **Citation chores** (authored/unverified numbers): #1, #2, #5, #7, #12, #16, #17, #18, #20, #21, #22, #23, #24, #25, #26, #27, #29, #30 (char yield), #31 (fire-tier peaks), #32 (tinder/moisture gates), #33 (pit seal retention).
- **Unbuilt rungs** (the fuel/tool staircase, model in `fuel.h`, stations/verbs pending): knapping (stone → edge); hafting (edge + head → pick/axe).
- **#28:** authored valley layout (deposit centers, radii, peak grades capped 0.5, `N_TIER`, linear falloff) — placeholders; tests assert only field shape.
- **Design questions Ross owns:** three-clause test as an owned DESIGN.md section; "teach the tool, never the wisdom" as a stated rule; §2b two-actor experiment; the re-panning verb; is `pick` a tax or a decision; how a panner's tilt becomes a gesture.
