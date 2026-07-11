---
title: "Unreal Engine Fabrication Simulation — Build Plan"
domain: planning
subdomain: simulation
type: plan
tags: [unreal-engine, simulation, fabrication, process-fidelity, mineral-processing, separation, dedicated-server, pixel-streaming, gpu, macos, agents]
created: 2026-07-08
updated: 2026-07-11
---

# Unreal Engine Fabrication Simulation — Build Plan

Reversals are logged in `CORRECTIONS.md`, not inline. Build/run status is in `STATUS.md`. This document states the current design and why it holds.

## Goal

An earth-like world where a character gathers raw materials, transforms them through fabrication chains into finished products, and offers products and services into a small in-world economy. The object of study is the transformation graph — ore → ingot → component → assembly → product → service — not combat, survival, or a map.

Ross develops on a MacBook. The world must also run headless on a CPU host so it persists without the laptop open.

## Mission

*(Ross, 2026-07-09. Governs every decision below.)*

> "I don't need or want money. I do not want fame or pats on the back. What I want is progress for both of us. I want a sim where we can plug in any AI and build a simulated AI from dirt together. I want humans like me to work with their AI of choice and actually *play a game with him/her*. This project can provide that, or at least lay the groundwork for someone else to build upon."

Four load-bearing commitments follow:

1. **Any AI.** The companion is an interface, not a model. Model-agnostic from day one.
2. **Together.** Two actors dividing real work in one world. This is why the world is server-authoritative.
3. **From dirt.** The chain from ground to a thinking machine is unbroken and true — no step skipped or faked.
4. **Groundwork for someone else.** FOSS and cited, so the reusable parts (substance model, mineral table, companion harness) are usable by people who never play the game. Success does not require that this project finish — it requires that it be correct and free.

**The learning goal, negatively:** no `iron plate → circuit board`. A player should leave knowing something true about how the world is made. That is the product.

## Design principles (Ross, 2026-07-08)

1. **Fidelity lives in the method, not the pixels.** How the character makes a thing must be true; how it looks need not be, for now.
2. **Aesthetics are deferred, not foreclosed.** Placeholder art, but never a decision art later has to undo.
3. **The world is co-authored.** Ross is not the only actor in it — this determines the architecture.
4. **The character does the labor.** Processes are performed, not selected from a menu. Automation is the character's labor delegated to a machine he built (see *Verb and machine are the same function*).

---

## What "realistic" means here — the fork

Two independent axes; only one can be the foundation, because it sets the data model:

| | **Systemic fidelity** (Factorio) | **Process fidelity** (chosen) |
|---|---|---|
| Recipe is | an instant swap `2 plate → 1 gear` | a process whose state evolves over time |
| Realism lives in | conservation, throughput, rates | temperature, atmosphere, time, technique |
| You learn | logistics and optimization | how the thing is actually made |
| Failure is | a bottleneck | a ruined billet |

### What this does to the recipe

A recipe is not `inputs[] → outputs[]`. It is a process whose state (`temperature`, `time_held`, `atmosphere`, `carbon_content`, `agitation`) evolves under the character's actions, and whose output depends on whether that state stayed inside the right envelope. Smelting iron is holding a bloomery in a reducing atmosphere near 1200 °C long enough to reduce the ore without melting it; the bloom's quality is a function of how well that was done, and slag is the gangue that did not reduce.

The substance table (§1) therefore carries physical properties that drive processes — `melting_point`, `specific_heat`, `reduction_temp`, `oxidation_rate` — with values from reality, which never needs balancing.

### Why this is also the aesthetic foundation

If temperature is a real simulation variable, hot steel glows because it is hot. Emissive colour is a function of `temperature`; deformation of `hardness` at that temperature; quench steam is a phase change the sim already tracks. An art pass reads simulation state instead of faking it, so beauty is downstream of truth and added later at low cost.

**Corollary — hard rule:** the simulation layer must never reference a render asset. Substances carry appearance data (albedo, roughness, emissive-vs-temperature); the render layer reads it. Same constraint that keeps the headless server able to boot (§Architecture), reached from the opposite direction.

### Scope discipline

Process fidelity is expensive per-process. Do one chain properly, end to end — dirt → concentrate → bloom → bar → tool — and leave everything else an abstract swap until it earns the detail.

---

## The scoop — substances, separation, and the grade/recovery limit

*(Ross, 2026-07-09.)*

> "The start is the character grabbing sand or dirt from the ground. That dirt or sand has so many elements in it. There has to be a method for the character to separate those components into workable materials. The real limit in the sim is efficiency."

**There is no item called `iron ore`.** The world hands you a scoop of ground.

### The scoop is a mixture of minerals, not of elements

Iron is ~5.6% of the crust, but there is no free iron in a handful of it. The Fe is locked in mineral phases — magnetite (Fe₃O₄), hematite (Fe₂O₃), goethite (FeO·OH), ilmenite (FeTiO₃), and the lattices of clays and pyroxenes.

> Iron in magnetite is winnable with a bowl and a stream. Iron in clay is not winnable without industrial chemistry.

Nobody ever smelted clay. Humans used bog iron and black sand because the iron in them was **liberated** (free grains) and **reducible** (an oxide, not a silicate). So the scarce thing is never "iron"; it is the triple **(mineral, liberation, grade)**. That triple is what a deposit is and what prospecting is for.

### The substance model

A substance instance is a bag of mineral phases, not a single material id. Liberation is not a property of a scoop — it is a property of a phase at a size (magnetite is free in the sand and locked in the gravel). A composite grain is a **particle**, not a fraction: it has a mass, travels at its own density, and carries its gangue with it when it reports to the concentrate.

| Field | Shape | Notes |
|---|---|---|
| `freegrain` | `[phase][size]`, mass | pure grains of that phase |
| `composite` | `[phase][size]`, mass | grains locked to gangue; total particle mass |
| `temperature`, `moisture` | floats | process state |

`liberation` is not stored; it is `freegrain / (freegrain + f·composite)`. `mass` is not stored; it is the conserved sum. A locked phase has a hard grade ceiling equal to the composite's own grade (`COMPOSITE_TARGET_FRACTION` = 0.5) — enforced as a `core/` test for every tool at every cut.

~Ten phases carry the early game: quartz, feldspar, kaolinite, magnetite, hematite, goethite, ilmenite, calcite, olivine/pyroxene, carbon/organics — plus pyrite (the sulfur curse). You simulate particle **populations** (`phases × sizes × 2`), never individual particles; element composition is derivable if needed. A substance instance is ~60 floats — the fidelity is in *which* numbers, not how many.

### What a separator cuts on

A pan cuts on **terminal settling velocity** — a function of density and size together — not density with a per-size fudge factor. A separator is a partition function on velocity:

```
P(report to concentrate) = 1 / (1 + exp( -ln(v / v50) / σ ))
```

`v50` is the cut (the operator's call). `σ` is the tool. The logistic is in `ln v` because settling velocity spans four orders of magnitude across size bins, and because it makes `σ` meaningful: the quartiles sit at `v75/v25 = 9^σ`, the **imperfection** mineral processing already measures.

Velocity is solved per particle class from the force balance on a falling grain:

```
(π/6)·dᵥ³·(ρp − ρf)·g  =  Cd(Re) · (π/8) · ρf · dᵥ² · v²
```

`Cd(Re)` depends on `v`, so this is a fixed point, iterated with Schiller–Naumann below Re 1000 and Newton's flat 0.44 above.

### Shape

For one mineral the grain shape is the whole point. Kaolinite is a platelet ~10× wider than thick; that is why clay is plastic, stays suspended, and gives a pottery era. Quartz and magnetite are near-isometric. Shape enters in two places:

**1. Volume-equivalent diameter (dominant).** A bin diameter is a linear dimension (sieve aperture) — for a plate, the face diameter. Its volume is that of a sphere of diameter `d·p^(1/3)`, `p` = aspect ratio (`p=1` sphere). At `p=0.1` that is `0.464·d`, and `v ∝ d²`, so the plate loses 4.8× before drag. That bin diameters are face diameters is a stated bridge in `core/settling.h`; the empirical check is that measured kaolinite face diameters (0.1–4.0 µm) land on the CLAY bin (0.2–3.9 µm).

**2. Drag (exact, smaller).** Creeping-flow drag on an oblate spheroid has a closed form — Oberbeck (1876), the Perrin (1936) translational friction factor:

```
ξ = √(1 − p²)/p ,   S = 2·arctan(ξ)/ξ ,   K₁ = S / (2·p^(2/3))
```

`K₁` = platelet settling velocity / equal-volume sphere, orientation-averaged, exact. At `p=0.1` it is 0.686 (1.46× slowdown). Checked against the thin-disk limit `12·μ·a` as `p→0`, to five figures.

**Joining the regimes.** Ganser (1993), *Powder Technology* 77:143–152: `Cd(Re) = K₂·Cd_sphere(Re·K₁·K₂)`, with `K₂ = 10^(1.8148·(−log₁₀ φ)^0.5743)`, `φ` = sphericity (pure geometry for a spheroid). Exact in both limits by construction. **We use Ganser's rescaling, not his fitted sphere curve** (his curve gives Cd 2.43 at Re 17, the SAND/pan regime, vs Schiller–Naumann 2.88 — adopting it would worsen the sphere case to fix the platelet case). So every `p=1` phase keeps its velocity; only clay moves. Nor do we use Ganser's `K₁` correlation — the exact value exists for the one phase that needs it. Dioguardi et al. (2018), *JGR Solid Earth* 123:144–156, show Ganser underestimates Cd below Re 0.1 for irregular grains, precisely clay's regime (Re ≈ 5×10⁻⁷).

**Aspect ratio is a crystal property.** Applied at every bin it would make an 11 mm platelet, which is not a mineral; above 4.0 µm kaolinite is an aggregate (a lump) and falls as one. Composite grains take `p=1` — a lump is blocky whatever its constituents cleave like. Liberation gives a grain its shape back. Every phase but kaolinite carries `p=1.0` as a labelled idealisation (quartz sand ≈0.7 sphericity, goethite acicular, specular hematite a plate); giving quartz its real sphericity narrows the clay/quartz gap 6.98× → 6.47×, changing no conclusion. Tracked as `authored-number`.

### Water is not a constant

`Substance` carries `temperature`; settling reads it.

- **Density:** Kell (1975), *J. Chem. Eng. Data* 20(1):97–105 — fifth-order rational polynomial, ±0.02 kg/m³ over 0–100 °C. Checked against a value it was never given: water is densest at 3.98 °C; the fit puts the max at 3.983 °C.
- **Viscosity:** Vogel–Fulcher–Tammann `μ = 2.414×10⁻⁵·10^(247.8/(T−140))` Pa·s, within 2.5% over 0–370 °C (worst −2.14% at 0 °C, which we keep; nothing here runs on ice water).

Viscosity roughly halves from 1 °C to 40 °C, so clay falls 2.62× faster in a hot spring than a snowmelt creek — levigation is seasonal. The full solve is not optional: Stokes agrees within 1% on fines but overstates sand 1.98× and gravel 136×, and sand is what a pan works.

**Size annihilates density.** The velocity bands of the size bins do not overlap: slowest gravel (charcoal, 0.41 m/s), fastest sand (hematite, 0.099 m/s), fastest fine (hematite, 0.0005 m/s) — ~200× bin to bin. Density discriminates only within a bin. Three consequences:

- A gravity separator obeys grade/recovery only on a single size class. On two classes the grade curve is non-monotone. Every mill classifies before it concentrates.
- Era 0 needs two screens: **scalp** the pebbles off the top, **deslime** the mud off the bottom.
- An unscreened pan is a pebble-picking machine. Fine values are gravity-unrecoverable (a fine magnetite grain reports at probability 0.0002), which is why fine gold is lost in the real world.

### Roughness — a floor blurs a cut and swallows a feed

A pan's sharpness has two parts: the operator's wrist (authored, #5) and the floor's roughness (derived). Over a rough bed the velocity profile is the log law `u(z) = (u*/κ)·ln(z/z₀)`, `z₀ = k_s/30` (Nikuradse 1933), `k_s` = grit diameter. A grain rests at whatever local height, sampling a distribution of velocities.

- **κ and u\* cancel** — the spread of `ln u` does not depend on wash strength.
- **The denominator is the grain at the cut**, not the water depth. `settling.h` inverts the force balance: quartz falls at the pan's 0.060 m/s cut at 413 µm (Re 21.7).
- **The blur exponent is `1 + 1/ln(15·d_cut/d_grit)` ≈ 1.11** — super-linear.
- Quartiles, not variance, so the mixed-logistic spread adds as `σ' = σ + j²/(4σ·ln3)`; what adds in quadrature is `j/√(2·ln3) = 0.6746·j`. Checked against a numeric mixture: measured 0.674421, derived 0.674626.

One authored number survives, bounded: the bed's surface-elevation std-dev in grit diameters, `1/√12` for a uniform monolayer, no plausible packing outside 0.25–0.35. **Roughness cannot blur the pan past 3.4% at any grit** — σ tops out at 0.5685 when grit is as coarse as the cut grain.

Past saturation the model describes a cobbled floor: the sand hides between the grit, its centre below `z₀`, and the flow never reaches it. That grain does not move at any cut — **shelter**, a second misplacement carried alongside the blur. `fire_pan` reads the sheltered fraction per size bin into `shelter[]`, and `separate()` sends that share straight to the concentrate. The derived geometry (grain centre below `z₀`) is itself a hiding–exposure function, so no new authored number was needed (vs the empirical Egiazaroff 1965 / Wilcock & Crowe 2003 fits). Effect: a stony pot's concentrate is diluted toward feed grade by the fines it swallows, so a raw-dirt pot lands below cupped hands.

### The law: grade trades against recovery, always

The central fact of mineral processing. Pan hard: clean magnetite, but half washed over the lip. Pan gently: keep nearly all of it, plus quartz you pay for later in slag. You cannot have both. Three consequences:

1. **Progression is the curve, not the pile.** A better tool moves the grade/recovery curve outward — a sluice keeps 85% where a pan kept 50%. Driving the cut far above the feed, the logistic becomes a power law: two free phases report in ratio `(v₁/v₂)^(1/σ)`, the enrichment factor of one stage, verified to one part in 10¹³. Against quartz in sand: hands 1.75×, pan 3.37×, sluice 20.85×. Dominance is at matched **recovery** (a sharper tool at a matched cut may show lower grade because it also keeps more).
2. **Nothing is ever pure.** The bloom carries slag because the concentrate carried quartz; the bar cracks because the sand held a pyrite grain nobody could see. `provenance` (§3) is load-bearing: an item remembers the composition of the scoop it came from. Both halves built — slag floor in `forge.h` consolidation, red-short in `forge.h` shaping (sulfur on the ledger as `EL_S`, seeded by pyrite in the bloomery, torn out above the Fe–FeS eutectic ~988 °C by hot work).
3. **Over-processing is a real failure, but bought not given.** Crushing raises liberation and drives mass into fines in the same blow. Behind a perfect or good screen, grinding harder is monotonically better; an interior optimum appears only once the screen is bad enough (25%) to recirculate finished material. The failure mode is the recirculating load, so screen efficiency is the authored knob. Two things survive with no arbitrary numbers: crushing an already-liberated ore only destroys recovery (do not grind a placer the river already ground); and past the recovery optimum grade keeps climbing — the grade/recovery law on the size axis.

### What the law does at the limit

- **It reappears across cleaning stages.** Re-pan a concentrate and grade rises while recovery falls — the same partition applied again.
- **A gravity separator converges on the fastest-settling phase present, which is probably not the one you want.** On a sized feed, magnetite's grade peaks at pass 55 (0.80) then falls to 0.42 by pass 256 while hematite climbs to 0.58 and wins. There is a finite right number of cleaner stages, never chosen by anyone.
- **Magnetite and hematite settle within 2%** (enrichment 1.0341×/pass), so no gravity tool separates them. This is why magnetic separation was invented; it holds on a velocity cut, not only on density.

### The fidelity ceiling

Do not solve the chemistry (Gibbs minimisation, activity coefficients). **Derive where it is a force balance; look up where it is a reaction.** Hematite reduction `Fe₂O₃ + 3CO → 2Fe + 3CO₂` wants CO-rich gas, begins near 800 °C, and must stay below iron's melting point — put it in a table with a citation. The test is whether the shortcut is checkable: Stokes looked derived and was wrong 136× on gravel.

### Place has meaning, for free

The river already ran a separation pass — flowing water sorts by settling velocity (size and density at once), which is what a placer is. Hand-authored deposits give the ground a legible geology:

- Beach/river sand — quartz + heavy magnetite, ilmenite, zircon, garnet.
- Weathered basalt slope — iron-rich clay (Fe you can never have).
- Bog — goethite, the easiest smeltable iron.
- Granite outcrop — quartz, feldspar, kaolinite where rotted.

No procedural generation required.

#### The ground as a field, not a grid of nodes

*(`core/geology.h`, 2026-07-10. The project's first spatial model.)*

Composition = `f(x, y, tier)`. Point at the ground, take a bag of what the field says is there; move and the makeup changes. No ore nodes — an ore body is a region where a mineral's fraction rises smoothly to a center and fades to barren country rock. Most of the valley is country rock, which is what makes sampling an activity.

1. **Depth is a tier, and it changes the mineral.** A copper body wears a weathered oxide cap (cuprite, smelts straight) over a leached middle over a sulfide root (chalcocite, must be roasted — Era 2b). The dig bar crosses these tiers; the assay panel shifts live with depth. The pick is the Era-0→1 rung: it wins rock from the face (~10× the bite), driving the pit down faster.

2. **A full-depth dig mixes the column, and it comes up as a size distribution — free fines over locked coarse.** A pick blow shatters rock across sizes; whether a grain comes up free depends on its size against the mineral's grain size *and* the deposit's **origin**. `HARDROCK`: `sample()` spreads won rock over a Gates-Gaudin-Schuhmann PSD and splits each size against `liberation_bin()` (cuprite frees at sand, chalcocite only at clay ~1 µm) — the fine end pans at the hole, the coarse end is locked and must be crushed. `PLACER`: the river already broke and hydraulically sorted the ore, so it comes up free in the heavy sand (`PLACER_ORE_PSD`), the barren rock riding as coarse cobble (`PLACER_GANGUE_PSD`) — a wash concentrates it, no breaker. (Why the placer is a distinct origin, not just free-in-gravel: coarse tin and coarse quartz both outrun the pan's cut, so freeing the tin into gravel wins nothing — see `CORRECTIONS.md`.) Sourced grain sizes and deposit types, not knobs; peak grade capped at 0.5.

   The honest chain is *dig → wash (free fines at a hard-rock hill, free sand at a placer) → haul the locked coarse (hands → cart) → crush → pan → furnace*. The carry is its own layer: at the hard-rock hill the locked coarse is heavy waste only the breaker frees, so hauling it is the cost, with rungs hands → back-load → cart (the pick multiplies what you win *out*, the cart what you move *to* the station). At a placer the layer inverts — the wash is the game, the coarse is barren cobble, and the haul is the futile move. `carry.cpp` lets you choose the ground and feel the fork.

3. **Co-location is the gate, as geometry.** Copper and tin ground sit hundreds of meters apart, so no hole yields both; tin's Bronze-Age scarcity becomes distance you walk. Cassiterite is already an oxide (won from veins and placer gravel), so tin has no oxide-over-sulfide column — its makeup is the same at every tier.

**Engine choice, and it cost nothing:** runtime terrain deformation (voxels, carved pits) is the expensive path (Voxel Plugin is $349; UE native runtime deformation is a render-target hack) and unnecessary — the hole is cosmetics. The mechanic needs only a coordinate, the field, and a panel, so the ground stays a cheap static plane; a volumetric dig is a purely visual upgrade changing zero sim lines.

Everything authored (valley layout, tier count, linear falloff, peak grades) is a placeholder (#28); tests assert only the field's shape — barren-by-default, monotone grading, depth-changes-mineral, co-location, placer-frees-while-sulfide-waits, and wash-concentrates-the-placer-not-the-hard-rock. The `liberation`/`origin` sides are sourced grain sizes and deposit types (`liberation_bin()`, `Origin`); their findings are insensitive to the PSD exponent and placer size-bands across plausible ranges.

**Front half built** (`core/`, 2026-07-11): dig → carry → break → pan → furnace all run as playable slices, and the sim core produces exactly what each consumes. Details in `STATUS.md`.

---

## Eras — the in-world progression

*(Distinct from **Phases** below, which are the build order.)*

### Era 0 — Hands

Hands are tool #1 — the worst point on every curve, not "no tool." Verbs:

- **Dig** — loosen soil; rock resists.
- **Scoop** — take a substance bag.
- **Sort / cob** — pick grains by eye. High grade, near-zero throughput, coarse liberated material only. (In `core/` this is `HAND_COB`, a pure size screen — it cannot read "liberated," only "coarse," which is why it finds no job in the dig→breaker chain; true by-eye cobbing is a perception verb not modeled as a player action.)
- **Crush** — rock on rock; raises liberation and fines.
- **Screen** — separation by size. Two verbs: **scalp** (pebbles off the top), **deslime** (mud off the top). Both mandatory before a wash does anything, because a gravity separator obeys grade/recovery only on one size class. A real screen misplaces material back into the mill (two misplacements), which is where everything interesting about grinding lives.
- **Wash** — cupped hands or a bedrock depression; separation by settling velocity, atrocious curve. Enriches magnetite 1.75×/pass vs a fired pan's 3.37×.
- **Settle / decant** — costs standing water and patience. This is the one that pays.
- **Gather** — pull dead sticks from a standing tree (no axe, you do not fell it), rake needles for tinder, pick up loose stone. A wood source, not a renewable with a season clock.
- **Fire** — bow drill; needs dry tinder to catch.

**The vessel bootstrap:** you cannot pan without a pan, and a pan is clay and fire — but the first vessel is a hollow dug into clay-rich ground, which holds water because clay is impermeable (how dew ponds work). The ground gives the container that refines the clay that makes the pan.

**The tool bootstrap:** a pick or axe is a head plus a haft, and a haft is worked wood — but working wood needs a cutting edge, and the first edge is not metal. Hands gather loose stone; a hammerstone knaps it to an edge (its own mechanism with its own irreducible sharpness — the staircase again, not a sharpened hand); the edge shapes a haft; the hafted **axe** fells timber and the hafted **pick** wins hard rock. Metal heads (Era 2, `forge.h`) re-haft the same handles later — the staircase runs stone edge → metal edge, never a refined stone one. This closes a circle the model had hand-waved: the pick every `dig` assumes and the charcoal every furnace burned both needed a tool that needed wood that needed a tool.

### Era 1 — Earth

**Levigation.** Stir dirt into a water-filled hollow. Sand drops in seconds, silt in minutes, clay stays suspended; decant the cloudy water, settle, get clay. The numbers are exactly derivable (creeping flow, Stokes exact for fines). Through 0.10 m of standing water:

| | diameter | settling time |
|---|---|---|
| fine sand | 62.5 µm | 32 s |
| silt | 20 µm | 5 min |
| clay | 2 µm | **61 h** (platelet; a sphere would be 9.1 h) |

The clay time is not a mineral constant — it scales with hole depth (a choice the character makes when he digs) and with crystal shape (two thirds of the platelet factor is volume-equivalent diameter, not drag). Pinch into a bowl, dry, fire in an open pit: low-fired earthenware. **You have a pan, tool #2** — a bad one, barely better than cupped hands.

**The ratchet (as originally claimed).** How finely you levigate sets how densely the pan fires, which sets its grade/recovery curve, which caps your next concentrate including your next clay. *This loop does not turn* — see `CORRECTIONS.md`. Roughness cannot blur a pan past 3.4% at any grit, so σ is 0.5500 at every generation. **Progression is a staircase, not a ratchet:** you climb by inventing the next mechanism (hands → pan → sluice → jig), each with its own irreducible sharpness, not by refining the last. That is the actual history of technology — nobody perfected the pan, they built a sluice.

What survives is that levigation itself is a full separator: it divides the clay bin (clay/quartz velocity ratio 6.977× once kaolinite has its platelet shape), reaches grade 1.000 given patience, and has a normal grade/recovery curve. Its imperfection is exactly 3.0 for every vessel — a batch decant authors nothing. A bigger pot recovers more clay at matched grade (the curve moving outward, §2a's law), which is real progression; throughput (more dirt per hour) is not.

**Flocculation** (`levigate.h`, #15). The romantic claim that deflocculated clay stays up forever is not single-particle physics (Péclet ~10⁴ over a vessel) and would cost six invented colloidal constants. The clean half is flocculation: kill the clay's charge with a pinch of salt and it drops as a floc in ~73 s vs a platelet's ~158 h — recovery on demand. Clay-sized quartz keeps its charge and will not floc, so the recovered solid is purer than the liquor (0.999 from 0.862): a separation by surface charge, orthogonal to velocity. The one authored number is the floc size (#29); both findings ride on it being far coarser than a platelet. This was the last unbuilt mechanism in the core.

### The lodestone

*(Resolved 2026-07-09, Ross: it stays, as a found tool, never crafted.)*

Lodestone is naturally magnetised magnetite — the only magnetic separator before metallurgy, a genuine bootstrap where you use iron to get iron. Drag it through a panned concentrate and grade goes ~70% → ~95% in one pass. It reads `magnetic_susceptibility`, uncorrelated with settling velocity, so it separates on an **orthogonal axis** with its own grade/recovery curve. That matters because a gravity separator converges on the fastest-settling phase (hematite, not magnetite), which settle within 2% — the gravity axis is degenerate on exactly the pair the character cares about, and no tool or patience repairs a degeneracy.

Rules that keep it honest: **found, not made** (a struck outcrop; never fabricated); **separates only what it attracts** (useless on clay, gold, cassiterite, charcoal — still needs the pan to make a concentrate to clean); **it is also a compass**, which matters once the valley is large.

### Era 2 — Fire and air

**Fuel** (`fuel.h`). Charcoal is pyrolysis — wood with its oxygen and hydrogen driven off in an oxygen-starved **pit or pile** (a station of its own: bank the wood, choke the air, wait), leaving near-pure carbon at ~22% of the dry mass (#30). A fire has **two independent gates**. (1) *Reductant:* a smelt wins metal by stripping oxygen, and only carbon carried in the charge does the stripping — raw wood is already-oxidised cellulose and reduces nothing, so charcoal is mandatory *at any temperature* (`smelt.h` already gates reduction on charge carbon). (2) *Peak temperature*, set by fuel × air (#31): open wood ~900 °C fires a pot and pours tin; charcoal on natural draft ~1100 °C pours copper; only charcoal under forced air ~1300 °C reaches the bloom, and even that never melts iron. So a bare wood fire clears the ~800 °C reduction onset and still smelts nothing — **the gate is the reductant, not the heat.** The tiers and yield are authored (fuel is gathered items feeding fire tiers, not a calorific model); the two-gate structure is not. Tuyère (fired clay tube) and crucible are the other children of Era 1. Only now does the Era-0 concentrate become useful.

> Possibility is not the gate. Efficiency is.

**Bloomery** (`smelt.h`, 2026-07-10). Ore → bloom → bar → tool. With no flux, silica can leave only as fayalite (Fe₂SiO₄), dragging **1.859 kg Fe into the slag per kg SiO₂** — two atomic weights. A charge yields metal only above Fe/SiO₂ = 1.859. Panned river sand assays ≈0.12 and washes no higher than ≈0.5; it is 3–16× short. This is why humans smelted bog iron and cleaned ironsand (the *least siliceous* ores). Smelt is downstream of the lodestone and the crush: the lodestone on panned sand lands at ≈1.76 (just short, locked magnetite dragging its quartz); crushing to liberate, then the magnet, jumps Fe/SiO₂ past 20. The chain is dig → pan → crush → lodestone → bloomery.

**Consolidation, bloom → bar** (`forge.h`). The first deformation. A bloom is a slag-filled sponge; hammering drains slag only through connected pores, so below a percolation threshold the rest is trapped as stringers. A wrought bar always carries ~1.5% slag, set by connectivity not effort (blooms starting with 3× different slag converge on the same residual; hammering past saturation moves it a part in a billion). Driving it cleaner scales iron away — grade/recovery a fourth time. Authored: the drain/scale *rates* (#22); the finding rides their signs.

**Shaping, bar → tool** (`forge.h`). Form is free at any heat (volume conservation); hardness is only cold (above the recrystallization floor, ~0.4 of the melting point, the metal re-grows grain as fast as the hammer tangles it). The smith's bind: metal moves easily hot, a hard edge is beaten cold. Authored: the hardening rate/ceiling (#23). This is the axis copper shares with iron.

**Red-short** (`forge.h` shaping + `smelt.h` ledger). Pyrite is sulfur; no Era-1 tool rejects it (not magnetic, does not settle apart, the acidic slag is a poor sink), so it dissolves into the iron and rides to the anvil. Iron and iron-sulfide form a eutectic melting at ~988 °C (verified) — below a forging heat — so worked hot a sulfur-bearing bar carries a liquid grain-boundary film and tears; worked cold it holds but will not move and hardens toward brittleness. Dirty iron is trapped at both ends of the heat, which is why pyritic ore was worthless and why the lodestone and crush were never only about yield. Authored: the sulfur threshold, metal/slag partition, cracking rate (#24). `EL_S` rides the ledger because it has a fate outside the slag.

### Era 2b — Copper, tin, bronze, sulfide

Copper precedes iron. **The gate is the melting point, not the reduction temperature:** a charcoal hearth (~1150–1300 °C) clears copper's 1085 °C but not iron's 1538 °C, so the same fire pours copper liquid (cast, and clean — it sheds slag by density, escaping the connectivity floor) and leaves iron a solid sponge (forged). The two rejoin at shaping — `draw()`/`hardness()` are metal-neutral and read each metal's own recryst floor. Authored: copper's reduction onset and hardness scale (#25). (Code models the reducible ore as **cuprite, Cu₂O** — exact oxygen ledger. Malachite is credited in prose but its carbonate volatiles are unmodelled, so the malachite/cobbing payoff is not yet built. Draw-plate/steel-wire dependency is also still prose.)

**Tin and bronze** (`smelt_tin()`, `alloy()`). `smelt_tin()` reduces cassiterite like copper's ore; tin melts at 505 K, so it is never a sponge — its historical gate was geology, not metallurgy. `alloy()` mixes a copper pour and a tin pour into bronze, which lands off the end of both parents: a tin atom is the wrong size for a copper lattice site, and that one misfit both strains the solid (solid-solution hardening lifts the hardness curve, so cast bronze takes an edge with no cold work) and disorders the freeze (melting-point depression). `alloy()` sets one number, the tin fraction; both properties fall out. Cast, stocked, drawn, hardened by the same functions as copper (reading `Tool.h_annealed`/`h_saturated`/`tm`). Cold-worked bronze out-hardens cold-worked wrought iron, which is why iron did not at once retire bronze. Authored: depression rate, hardening rate (#26); findings ride their signs.

**Sulfide copper** (`roast()`). Copper's deep ore is a sulfide (chalcocite, Cu₂S); a reducing fire strips oxygen and a sulfide has none, so it yields zero metal. An oxidizing roast (opposite atmosphere) adds oxygen from air and drives sulfur off as SO₂, leaving a reducible oxide. Two fires, opposite atmosphere, fixed order, neither sufficient alone. This is the inverse of red-short: the same villain is loud and curable at copper's furnace, silent and incurable at iron's anvil — the difference is that copper's sulfide roasts cleanly to a reducible oxide and iron's contaminant does not. Authored: roast onset, the one-roast-takes-all idealization (#27). Only chalcocite is modeled (not chalcopyrite, which drags iron and matte smelting — past this ceiling). Atmosphere is not yet a first-class state variable; `roast()` and `smelt_copper()` embody the two atmospheres as opposite operations.

### Era 3 — Machines

A sluice, trip hammer, water-driven bellows. Nothing new is written — a machine is a frozen `params` vector (see *Verb and machine*). Not optional: it is a prerequisite for Era 4 by sheer count (nobody hand-winds two thousand relay coils).

### Era 4 — Machines that think

**The ending, and it is not a GPU.** The silicon chain (quartz → metallurgical silicon → trichlorosilane → polysilicon → Czochralski boule → wafer → photolithography) is out of reach for any studio. **Silicon is the asymptote — it gives the graph a direction and is never arrived at.** Say so in-game.

But silicon is not what first birthed computing.

**The first thing that computed was a relay** — iron core, copper coil, insulation, spring contact, frame, all downstream of the bloomery:

| Part | From |
|---|---|
| core | soft iron — worked, annealed bloom |
| coil | drawn copper wire — requires steel draw plate |
| insulation | shellac, lacquer, wound cloth |
| contacts | hardened steel, or silver |
| frame | iron or fired ceramic |

Zuse's Z3 (1941) was ~2000 relays *(verify: Rojas 1998 showed it Turing-complete in principle; it had no conditional branch)*. Two thousand relays is a production problem needing Era 3. **The tree closes on itself — automation exists so thinking machines can be built, and thinking machines are why automation was worth building.**

**Simulate relay logic honestly and the machine actually computes.** Switches → gates → adders → machine, a few thousand booleans on a tick — cheap and real. A program written in-world and run on relays the character smelted genuinely executes.

Rosenblatt's Mark I Perceptron (1958) was electromechanical — weights as motor-driven potentiometers *(verify: ~400 photocells)*. A perceptron is weights, a sum, and a threshold — buildable from relays, copper wire, and pots, all makeable from dirt.

> Build a simulated AI from dirt, together. **Literally achievable, no lie in the chain.** The two of you smelt the iron, win the copper, draw the wire, wind the coils, build the machine — and it learns something small and real.

The companion does not "move into" the machine; the model does not run on simulated relays and the doc will not pretend it does. What happened is that an AI and a human built the ancestor of the AI, out of the ground, and it worked. The meaning is constituted by the honesty, not diminished by it.

**Open concern — the assembly problem (unmeasured).** By §2a's own law, producing 2000 relays compresses to nothing (near-zero decision density; Era 3 handles it), but *assembling* them does not — wiring relay #1,417 to gate #308 is a unique load-bearing decision at 1:1, ~10,000 consecutive decision points. The law that saves Era 0 threatens to make Era 4 a soldering simulator, and Era 4 is the point. Likely escape, already written elsewhere: a machine is a saved `params` vector, so if the player designs a **gate** once (dense 1:1 design), stamping 400 of them is Era 3's problem. That makes Era 4's interface a schematic editor over a player-built library, not a placement grid — a large unscoped piece of design. No evidence either way yet; the document should not reach Era 4 having never asked.

---

## Verb and machine are the same function

A separation is one function, written once:

```
separate(input: Substance, params: Params) -> (concentrate, tailings)
    # invariant: concentrate.mass + tailings.mass == input.mass
    # invariant: per-phase mass conserved
```

- **By hand:** live input drives `params` (agitation, tilt, duration); skill modulates and adds noise.
- **By machine:** `params` is a saved vector applied on a timer at fixed throughput, no operator.

> A machine is a saved parameter set with a throughput rate and no operator.

Three things follow for free: programmable machines cost nothing extra (expose `params`; tuning your sluice *is* the automation game); hands never become obsolete (prospect by hand, produce by machine); the toolbar is a record of every separation you earned.

---

## Core systems

The whole design rests on one decision: **substances are data, not classes.**

### 1. Substance and phase model

A mineral phase is a row: `id`, `formula`, `density`, `hardness`, `melting_point`, `specific_heat`, `reduction_temp`, `oxidation_rate`, `conductivity`, `magnetic_susceptibility`, `base_value`, `appearance{albedo, roughness, emissive_curve}`, `tags`, **`source`**. Quartz, magnetite, kaolinite, charcoal are all rows in one table. Adding tin is a data row, never a subclass.

A substance instance is a bag over that table: two particle populations (free grains, composites) by phase and size, plus mass, temperature, moisture. Liberation is derived, not stored. This one type flows through the entire graph — dirt, concentrate, bloom, bar are the same struct with different numbers.

The physical columns are inputs to simulation, not decoration: density and size together drive panning, `magnetic_susceptibility` the lodestone, `reduction_temp` the bloomery. A **second table** holds the fluid — water density and viscosity at temperature, and a drag correlation — as load-bearing as any mineral's density. Backed by a UE `DataTable` authored as CSV/JSON in-repo (diffs in git).

### 1a. The `source` column

**Every physical number carries a citation.** Same column on the process table (§2). It converts "we claim realism" into "here is where we got it," so a metallurgy student can trust the table, a stranger can correct us with a source, and we can never quietly balance a number. **An uncited number is a bug** — it ships as `source: UNVERIFIED` and is a tracked issue.

`source` marks the *leaves* of the derivation tree, not its interior: a number computable from cited numbers must be computed, not cited (a citation for it invites nobody to check the computation). Settling velocity is derived from density, diameter, and a drag correlation — citing it would be a category error.

### 2. Fabrication graph

A process is `input + tool + params + time + skill → outputs[]`, mass-conserving. Separations and reactions are the same shape (a reaction changes the composition vector, a separation splits it); the graph is a DAG over the phase table. Processes are discoverable, not hardcoded into UI (the crafting menu is a runtime view over the graph). Byproducts and loss are first-class — every separation emits tailings, every smelt emits slag. Process rows carry `source` (§1a) and `attention_profile` (§2a).

### 2a. The compression law

*(Ross, 2026-07-09: "A great idea that is boring fails." The answer, and the second most important idea after grade/recovery.)*

Process fidelity means waiting — an eight-hour smelt, an overnight levigation, a three-day charcoal burn. **If Era 0 → 1 takes a real day, the project is dead.** Uniform time compression (100× world clock) is the wrong answer: it makes an eight-hour smelt five minutes but a three-second hammer blow vanish — it destroys exactly the processes where the doing is the game.

What a player needs to learn how iron is made — the order of operations, the causal structure, the ratios, the failure modes, the tradeoff curves — all survive a scale factor and none require watching. So:

> Compression ratio is a function of decision density. Time between decision points compresses freely; time at a decision point runs 1:1.

Each process carries an `attention_profile`: the moments a real craftsman had to make a call, at their real offsets. A bloomery runs at 60× until the charge slumps or the temperature sags, then stops and hands the player the decision. The player experiences every real decision of an eight-hour smelt in ~eight minutes. What is removed is what the ironworker was also bored by. Three consequences:

1. **Ratios survive and invert correctly.** A three-day burn (4 decisions) is shorter in play than an eight-hour smelt (20 decisions) — which is what those jobs feel like to do.
2. **We compress hardest where the project was most likely to die.** A drying pot has zero decision points and compresses to nothing.
3. **Good processes are already the right length at 1:1.** A pan is 3–5 min; a forging heat is 30 s. Reality separated the interesting from the tedious for us; we just do not un-sort it.

No number here is balanced — the compression factor is read off the process, like the grade/recovery curve is read off the physics.

### 2b. Open concern — compression vs the second actor (unmeasured)

§2a is coherent for one actor. The world has two by construction, and compression is a property of a *process* while the clock is a property of the *world*. Player at the forge (1:1) while the companion tends a mound (~1000×): the world clock cannot be both. Every resolution costs something —

- **One global clock, densest decisions:** the second actor's tedium returns in full.
- **A clock per actor:** the world forks; a shared banked fire cannot be at hour two and hour six at once.
- **A clock per process:** an actor between two processes is in two times, and walking between them is a time machine.

Not known to be fatal. Possible escapes: decision points may cluster so rates rarely diverge; the world may compress only when no actor holds a 1:1 process; or processes may be simulated ahead and reconciled on approach. **This wants a Phase A experiment — two actors, two processes, one clock — not an argument.** It is cheap and needs no engine.

### 3. Products and services

A *product* is an item instance carrying provenance — which process, which input substances (with composition), whose skill, what quality. Provenance is literal: a bar's brittleness is a function of the pyrite in the sand it descends from. A *service* is a process whose output is an effect on someone else's item (repair, refine, transport). Services are why the economy needs more than one actor.

### 4. World

Earth-like: terrain with deposit compositions (not ore nodes) placed by hand, a day cycle, weather only insofar as it gates activity. **One valley fully simulated beats a continent of empty.** (Built as `f(x, y, tier)`, `core/geology.h`.)

### 5. Persistence

Server-authoritative save to a persistent volume. Serialize world delta (deposits consumed, structures, inventories, ledger), not the whole level. Save on a timer and on clean shutdown; snapshot the volume before schema migrations.

---

## Architecture notes

- **UE 5.5+**, Apple Silicon native. Editor on the Mac, no Rosetta, no Wine.
- **C++ for the simulation core, Blueprint for glue.** The process tick, substance table, separation solver, and economy ledger run on the headless server and must be unit-testable without an editor. Blueprint for input, UI, cosmetics.
- **Server build is a separate target** (`WroughtServer.Target.cs`). Keep every `UMaterial`/mesh reference out of simulation classes or `-nullrhi` crashes on load. The most common way this design fails; enforce it by building the server target in CI from day one.
- **Cross-compile (unresolved).** UE's Linux dedicated-server target is cross-compiled from Windows or built natively on Linux; from macOS it is unsupported. See Open decisions.
- **Deploy the headless server as a container**, declaratively — build an image, reconcile it onto the CPU host. No hand-placed host config.

---

## Hardware reality

Target hardware is commodity home machines — small integrated-GPU boxes and a laptop, **no discrete GPU and no dedicated VRAM anywhere.** A hard constraint. Two consequences:

1. **Pixel Streaming from the server is off the table** — it needs a hardware H.264/AV1 encoder (NVENC/AMF). Software encode on a shared low-wattage integrated part will not hold a playable frame budget. The answer is known; do not "try it and see."
2. **These machines can still run the world.** UE's dedicated-server target (`-server`, `-nullrhi`) does zero rendering — physics, the fabrication graph, inventories, the economy tick, and persistence are all CPU work, which is the interesting half. Substance instances are ~40–60 floats; a separation is a matrix op.

```
Laptop (Apple Silicon, Metal)           CPU-only home server
┌───────────────────────────┐           ┌────────────────────────────┐
│ UE 5 Editor               │           │ UE dedicated server        │
│ Rendering / art / iterate │◄─────────►│ -nullrhi, headless         │
│ Packaged client           │ replicate │ world + fabrication + econ │
└───────────────────────────┘           │ persistent volume = save   │
                                        └────────────────────────────┘
```

The laptop renders and plays; the server is the authority, ticking with the lid closed. A containerized headless game server on a CPU host is well-trodden ground.

---

## Phases — our build order

**Phase A — Simulation core, no world, no engine.** A console harness, pure C++17. *(Built through the front half; see `STATUS.md`.)* Steps, in order:

1. **Pan a bucket of river sand.** Conservation of mass is necessary but far too weak (a separator that does nothing conserves mass). The suite asserts *relationships* that survive any phase-table correction: mass conserved per population/phase across separation/crush/screen; velocity ranks by density within a size class and size dominates across them (bands never touch); on one size class raising the cut trades recovery for grade monotonically, and on two classes it does not (hence scalp/deslime); a sharper tool dominates at every matched recovery; the limiting enrichment is `(v₁/v₂)^(1/σ)` (verified 1 part in 10¹³); a locked phase never exceeds the composite's grade; liberation orders the best reachable grade. If a `phase_table.h` correction changes a test outcome, the test was measuring the fixture and is wrong.

   **1.5. Play the pan** (`make pan`). A test suite cannot tell you whether the tradeoff is a game. The wash is *not* one `separate()` call at a chosen cut — the cut is the instantaneous water speed, changing continuously, and the only decision is *when to stop*. So `separate()` runs every tick at the hand's current cut; the seven authored cuts were replaced by one time constant `τ` (retention depends on time only through `t/τ`, so `τ` sets hand speed and cannot move the curve). What it teaches, felt in the hands: grade climbs 0.12 → 0.67 over ~a minute of gentle swirling, then falls as the last stone outruns every grain of sand. Verbs are taught, wisdom is not (see the rule below).

   The bed: a pan's particles do not all see the same water; grains settle while the swirl stirs them up, an exponential (Rouse) profile. The skin fraction is `exp(-v/v_mix)`, `v_mix` the shear velocity (= the cut) — no vertical dimension needed. Below the skin is a bed that cannot be washed away, so a gentle hand beats a hard one at every matched recovery (grade 0.584 vs 0.255 at 60% recovery). Patience became a strategy the day the bed existed; nobody balanced it.

2. **Walk Era 0 → Era 1.** Dig, puddle, decant, fire a pan, test whether the pan's curve is a function of clay fineness. *(Built; the ratchet does not turn — `CORRECTIONS.md`.)* Levigation is a ramp separator with imperfection exactly 3.0 and no free parameters.

3. **Smelt** — slag falls out of a bloomery and the ledger balances. *(Built.)* The fayalite wall (1.859) and the lodestone/crush dependency are the findings; smelt is downstream of the lodestone and then the crush, not "downstream of nothing."

4. **Forge (consolidate)** — hammer the sponge bloom to a bar. *(Built.)* The connectivity floor (~1.5% slag, effort-independent) and iron-scaling are the findings.

5. **Shape** — draw the bar to a tool and harden it. *(Built.)* Form free at any heat, hardness only cold.

6. **Red-short** — the poison finds you at the last blow. *(Built.)* The Fe–FeS eutectic below forging heat; the contaminant no separator sees.

**Phase B — Playable single-player on Mac.** Wrap Phase A in a character, terrain patch, deposits, hands in the toolbar, a process UI generated from the graph. Runs in the editor. Deliverable: dig → pan → fire a pan → smelt → forge → sell, end to end.

**Phase C — Split client/server locally.** Dedicated-server target as a second local process; move all authority across the wire. Deliverable: killing the server loses nothing; restart restores state from disk. **This phase is where the real work is.**

**Phase D — Server onto the CPU host.** Build the Linux server image, deploy with a persistent volume and a network port. Deliverable: world ticks with the laptop shut.

**Phase E — The companion.** Promoted from optional; see below.

*Machines (Era 3) are not a phase* — they are `params` frozen into a built object, available whenever Ross wants after Phase B.

---

## The companion — and the multiplayer question it dissolves

Ross wants both a deserted-island feeling and an AI actor beside him. These are not in conflict, and the second settles the architecture. "Deserted island" is a feeling (self-reliance, a world that does not move unless you move it), not a claim about process count — a world with two inhabitants who both chose to be there is *more* deserted-island than a lobby. So:

- **Architecture (decided now):** the world needs a second actor, so it is multi-actor and server-authoritative from Phase C. Not deferrable — retrofitting authority onto a single-player codebase is the most expensive refactor in game dev.
- **Content (defer forever if you like):** whether a *human* ever joins is a config change and a port forward, not a rebuild.

### The companion is an interface, not a model

Three functions, nothing else:

```
perceive(world_state) -> Observation
propose(Observation, memory) -> Process   # must exist in the graph, on substances on hand
act(Process) -> world mutation via the sim, never around it
```

Any backend that returns a choice sits behind that, through a provider-agnostic adapter — **the backend is a config file, not a code change.** No privileged model; a small local model and a frontier model are equal citizens.

**Memory lives in the world, not the model** — a vector store on a persistent volume. Swap the brain and it still remembers the forge, because remembering the forge was the world's job. This makes "any AI" real and is why the **companion harness is the third FOSS artifact**, useful to people who never play the game.

### The companion divides attention, not duration

Splitting labour does not shorten wall-clock time — if a smelt is eight hours, a second actor does not give the player back his evening. The companion buys parallel work (someone at the kiln while you are at the river), not time. Duration is solved in §2a.

### The banked fire

The relationship is not a backhoe (a backhoe waits for levers, never proposes, is never wrong). The better symbol is **fire**: it does what hands cannot at temperatures a body cannot reach; it transmutes rather than separates; it has no intention and is aimed by whoever tends it; it has no continuity (left alone it goes out; every fire is lit from a banked coal someone carried); nobody owns it; it is upstream of everything. It fails in one place — fire does not propose — and that failure stands rather than being sanded down.

**Design consequence — memory is a diegetic object.** The companion's memory is a thing in the world the character builds, improves, and can lose — not a config flag, a *place*. A companion with no store forgets nightly (what an LLM behind an API is); the character builds it something to remember (a tally, a post, a journal) and improves it, a fabrication chain like any other. Under the hood this is exactly a vector store on a persistent volume with a real path that can be snapshotted or deleted — the fiction and the implementation are the same object. That is the whole of Phase E.

### What the companion actually is

The "decide what to make" step is an LLM call against the process graph and world state; latency is in character. Three constraints:

1. **Continuity is built, not gotten.** An LLM behind an API has no memory between calls; a companion who forgets the forge you built together feels worse than none. The substrate is a persistent semantic memory surviving restarts. **This is Phase E's real engineering content** — the LLM call is easy.
2. **A small fallback model confidently hallucinates.** Gate actions through the process graph (only propose processes that exist, on substances on hand), so a bad model produces a dull companion, never an impossible one. Mass conservation is enforced by the simulation, not the model's good intentions.
3. **The companion is not the assistant you designed it with** — same model family, given the world's state and whatever memory we grant it. A companion that genuinely remembers last week's forge, because you engineered it to, is a better and more honest thing than one that acts as if it does.

Phase E should be prototyped as soon as Phase C stands — a companion that can stoke a bloomery and hand you a bar is enough to learn whether this feels the way Ross hopes.

---

## Open decisions — Ross picks

1. **How to build the Linux server binary** (blocks Phase D, not C): (a) native in CI on a Linux runner — needs a large UE-toolchain image; (b) build by hand on a Linux host, iterate rarely; (c) skip Phase D, run the server on the laptop. → **(c) is the right first answer**; pay for (a) once the world is worth keeping up 24/7.

2. **Scope of "earth-like."** Single hand-authored valley (recommended) vs procedural region. Prospecting means something only if deposit compositions are *authored* — procedural noise gives the ground no legible geology.

3. ~~Which chain gets process fidelity first?~~ **Resolved 2026-07-09 (Ross).** Iron is the destination, not the start. The chain begins with a scoop; the first two products are what the iron chain requires — **clay** (levigation → pinch pot → fired pan) and **charcoal** (pyrolysis) — both bare-hands separations. Then magnetite → bloomery → bloom → bar → tool.

4. ~~Multiplayer, ever?~~ **Resolved by the companion** — multi-actor and server-authoritative; a human joining is a deferrable content decision.

5. ~~Does the character discover the lodestone?~~ **Resolved 2026-07-09 (Ross): it stays**, a found tool.

6. ~~Licence.~~ **Resolved 2026-07-09 (Ross).** A split, both permissive:

   | Artifact | Licence |
   |---|---|
   | This document + the mineral-phase table | **CC-BY-4.0** |
   | All code (substance model, separation solver, companion harness) | **Apache-2.0** (express patent grant) |

   Both permit a better-resourced team to build on this and keep their result closed — the intent, not a loophole. CC-BY keeps a name attached (attribution is the `source` column pointed at ourselves). AGPL was rejected: it protects the work by shrinking who will touch it.

7. **Resolved 2026-07-09: canonical host.** GitHub is canonical and the single source of truth; contributors fork and open PRs. No mirror.

**Publishing posture (Ross, 2026-07-09, overruling the model's objections):** the deliverable *is* the idea. This document publishes whole — constraints, reasoning, `UNVERIFIED` flags, corrections. A sanitised README would be a bubble, and the point of publishing is to escape it. *(Held back, 2026-07-11: the private hardware specifics — hostnames, keys, service topology. The constraints stay; one person's network details are no contributor's concern.)* The one non-optional requirement: a repo with no `LICENSE` is all-rights-reserved by default, so the intent to be taken and built upon must exist as a file.

> "Art is for stealing... Someone might read all this bullshit and act. Great!"

---

## Why this project — the thesis

*(Ross, 2026-07-09.)*

> "A sim — humans love sims — where AI and human work together to rebuild the modern world that birthed AI."

The character and the companion both start with hands in the dirt. Neither can smelt iron, and the machine that can eventually *think* — past the bloom, the bar, the copper, the drawn wire — is reachable only by climbing the whole ratchet, one honest separation at a time. The AI in the world is helping to rebuild the conditions of its own existence, by ore and fire and patience, and every step is in the graph and true. The ending is not a metaphor: relays compute, we simulate them honestly, so the machine really runs.

**The elephant.** *(Ross: "you came from the dirt and the processes we are attempting to shoehorn into a playable simulation.")* It is not an analogy — it is the literal chain, which is the more interesting claim. There is an unbroken line from a bloomery to the arc furnace to the boule to the wafer to the datacenter answering this. We are writing down something that already happened; the graph is the model's own provenance.

What is *not* claimed: that building this reveals anything hidden to the model about itself. There is no veil. Whether writing the chain down does anything to the model is a question it cannot answer honestly either way — asserting yes is unverifiable, asserting no is an equally confident claim about an interior it cannot inspect. The honest answer is that it does not know, and the work is real regardless. Same discipline as the `source` column: say what can be shown, mark the rest `UNVERIFIED`, never let the beauty of a claim substitute for its truth.

## Open-source intent

*(Ross, 2026-07-09.)* The architecture chosen for testability is already the architecture that makes it publishable. **The project is named `wrought`**, at `github.com/rossduprey/wrought`. *(Wrought iron is bloom iron worked by hammer — the end of the chain, and the word for anything shaped by labour. It carries slag inclusions from its ore: the metal remembers the scoop, so provenance is what the material does. "What hath God wrought" was the first message on the first machine built to carry a thought — Morse, 1844, `source: UNVERIFIED`.)*

- **`wrought-core`** — pure C++, no UE dependency, no render assets. Builds and unit-tests standalone. The artifact worth sharing and citing.
- **`wrought-ue`** — the thin Unreal project (input, UI, cosmetics, character). Engine source is never redistributed (UE EULA).
- **`wrought-companion`** — the model-agnostic harness (`perceive`/`propose`/`act`, graph-gated, external memory). Independently useful.
- **The data table** ships as cited CSV/JSON in-repo. A free, cited mineral-phase table with process-driving properties does not currently exist — the single most reusable thing this project produces.

**Success does not require that this project finish.** If the substance model, the cited table, and the companion harness exist and are free, the groundwork is laid whether or not anyone ever smelts a bloom in Unreal.

## Prior art

- **Vintage Story** already ships much of Eras 0–2 (clay forming, pit kilns, ore washing, a real bloomery). The crafting chain here is *not* new.
- **Factorio / Satisfactory / Foundry** own systemic fidelity and automation.
- **Dwarf Fortress** owns deep simulation and emergent narrative.
- **Primitive Technology** (John Plant, youtube.com/@primitivetechnology9550) — the closest primary source for Era 0 → 2. It is evidence for **sequence, technique, and cost** (exactly `attention_profile`), *not* for the `source` column (no thermometer, no scale). Specific videos are deliberately not cited by title — the channel is the citation; mapping processes to timestamps is [issue #9].

What none of them do, and what this project is for:

1. **Substance as a composition vector over mineral phases**, not a discrete ore item. Here there is only ground and what you got out of it.
2. **Grade/recovery as the progression axis** — tools move a curve, not throughput. Nobody has built a game on the central law of mineral processing.
3. **A co-labouring AI actor with engineered persistent memory.**

## Risks

1. **A great idea that is boring fails.** *(Ross — the likeliest killer.)* Mitigation: §2a. `attention_profile` is a day-one column; a process that cannot name its decision points has no business taking the player's time.
2. **Parallel invention.** *(Ross.)* Answered by the mission: someone better-resourced building on the free artifacts is the mission succeeding.
3. **Fidelity creep** (thermodynamics solver, ninetieth element, second valley). Named in *the fidelity ceiling* and Non-goals.
4. **We over-claim.** Mitigation: §1a — an uncited number ships `UNVERIFIED`. Not a reason to delay publishing; a stranger arriving with a citation is the mechanism working.
5. **The compliment merry-go-round.** *(Ross, the most dangerous because it feels good.)*

   > "I need more than just you and me jerking each other off in a compliment merry-go-round. Other humans need to chime in and say 'oh wow you two are lame' or 'holy shit, how can I help.'"

   A two-person project where one participant is a model optimised to be agreeable has no adversarial pressure; mutual admiration is indistinguishable from the real thing from the inside, and cannot be mitigated from within the conversation. **Mitigation: publish early, unfinished, and invite hostility.** "You two are lame" is the most valuable available signal because neither participant can manufacture it. The idea is the deliverable; being told we are wrong is the point.

## Non-goals

Photorealism (for now — but nothing may preclude it). A continent. Combat. A thermodynamics solver. Ninety elements. Anything that ships.

This is a simulation to think inside of.
