# Corrections

Claims that turned out wrong. The project keeps wrong claims rather than erasing them — but here, not inline. `DESIGN.md` / `RUNBOOK.md` / `README.md` state current truth only; the history is here. One entry per reversal: claimed / wrong because / now. Newest first. Updated every turn a finding is overturned.

## 2026-07-12

**geology, dig — the pick is reach, not only rate.** Claimed: a tool sets how *much* rock you win of a given makeup, never the makeup, so a patient bare hand reaches any depth just slower (dig.cpp's rate-only pick; the "hands sample, the pick wins the load" framing). Wrong by omission: weathering makes the oxide cap friable but leaves the sulfide root fresh, competent rock a bare hand skips off — so the hand cannot win the deep ore *at all*, not merely slower. Now: `rock_competence(origin, tier)` is a wall (SURFACE 0.001 < MIDDLE 0.02 < DEEP 0.10, placer 0), and a strike wins a tier only if its blow energy clears it. Against real `haft.h` energies the bare hand (0.002) takes the cap and the placer and skips the root; even a crude sapling pick (0.198) wins the whole column — so roast-grade copper is gated behind the tool bootstrap, and the tin creek needs no pick. The old finding survives intact where both tools win: `sample()` is tool-blind, so a hand and a pick that both open a tier win the very same makeup. The tool changed reach and rate, never makeup.

## 2026-07-11

**geology, carry — free is not won.** Claimed: a placer's tin, freed into the GRAVEL bin, is won by panning. Wrong: coarse cassiterite (1.42 m/s) and coarse quartz (0.745 m/s) both far exceed the pan's 0.060 m/s cut, so panning the raw heap keeps 98% of the tin at unchanged 45% grade. Now: deposit `Origin` (HARDROCK/PLACER); a placer is river-sorted, ore free in the SAND. Wash lifts tin 45%→76%; the same wash at the hard-rock copper hill wins ~2%.

**geology — liberation is a function of size, not a scalar.** Claimed (earlier same day): a full-depth dig comes up all locked rock, nothing pannable at the hole. Wrong: a pick breaks rock across a PSD; the fine end of a placer is already free. Now: `sample()` splits each size against the mineral's `liberation_bin()`. (The still-earlier #28 shortcut — all ore free, size-independent — was the opposite error.)

**breaker — the verb after crush is the pan, not cobbing.** Claimed: freed coarse rock is hand-cobbed. Wrong: `HAND_COB` is a size screen; the dig delivers ore and gangue at the same size, and after a crush the freed ore falls fine while cob keeps the coarse — it throws the ore away. Now: the pan parts freed ore from gangue by density. (Cornish order was spalling → cobbing → bucking → jigging; cobbing was a pre-crush by-eye sort, not modeled as a player verb.)

## 2026-07-10

**#13 — grains are not spheres.** Claimed: all grains settle as spheres; clay/quartz velocity ratio 1.031×. Wrong: kaolinite is a platelet ~10× wider than thick; a face-diameter plate has the volume of a 0.464·d sphere, and v ∝ d². Now: ratio 6.977× (volume-equivalent diameter + exact Oberbeck/Perrin drag, K₁=0.686 at p=0.1). Overturned "Era 1 has a lodestone-shaped hole" — levigation divides the clay bin to grade 1.000.

**#13 — water is not a constant.** Claimed: settling computed at fixed 288 K. Now: Kell (1975) density + VFT viscosity, temperature-dependent; clay falls 2.62× faster in a hot spring than a snowmelt creek. Levigation clay time corrected 1 h → 9.1 h (sphere) → 61 h (platelet) through 0.10 m — a property of crystal shape and hole depth, not a mineral constant.

**#10 — the ratchet does not turn.** Claimed: finer clay fires a sharper pan; blurring a pan 10% needs 267 µm grit. Wrong: three compounding errors, 8.85× too large. Now: log law over a rough bed, exponent 1+1/ln(15·d_cut/d_grit)≈1.11; roughness cannot blur a pan past 3.4% at any grit. Progression is a staircase, not a ratchet. The real coarse-floor effect is shelter (#19), a second misplacement, not blur.

## 2026-07-09

**substance — a composite is a particle, not a fraction.** Claimed: liberation is a scalar, then a per-cell fraction. Wrong: a per-cell fraction lets a locked grain reach the concentrate without its gangue, overstating grade ~13% at liberation 0.5. Now: a composite grain travels as a particle carrying its gangue; the grade ceiling is enforced, not asserted.

**separate — cut on settling velocity, not density.** Claimed: separators cut on particle density × a hand-set per-size-bin efficiency. Wrong: three invented numbers per tool standing in for physics. Now: cut on terminal settling velocity (density and size together), Tromp partition, σ = imperfection.

**#3 — over-processing failure is recirculating load, not slimes.** Claimed: grind too far → unseparable slimes, a free day-one tradeoff. Wrong: behind a good screen, grinding is monotonically better forever; the optimum appears only with a ≤25% screen. Now: the failure mode is recirculating load; screen efficiency is the authored knob. (Re-measured after density→velocity: worse screen needed, 25% vs 50%.)

**magnetite/hematite degeneracy — restated on velocity.** Originally a density claim (5.26 vs 5.15 g/cm³). Now: holds on a settling-velocity cut too — enrichment 1.0341×/pass, peaks at pass 55 then hematite wins. Stronger than the claim it replaced.

**fidelity ceiling — amended.** "Never derive" → derive where it is a force balance, look up where it is a reaction. Test: is the shortcut checkable? (Stokes looked derived, was wrong 136× on gravel.)

---

Also preserved as code comments (index here if they move): two in `levigate.h`, one each in `fire.h`, `ratchet.cpp`, `test_separation.cpp`, `README.md`.
