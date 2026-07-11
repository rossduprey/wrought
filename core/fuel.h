#pragma once

#include "substance.h"

// Fuel: wood, charcoal, and the two gates a fire actually has.
//
// Before this file, charcoal was spawned. furnace.cpp handed the bloomery a fixed
// carbon bed and said so out loud -- "kept ample on purpose, so FUEL is never the
// binding constraint." That was a scaffold, and it hid the first fact of
// pyrometallurgy: the charcoal is not a convenience laid beside the ore, it is the
// reductant, and it is MADE, not found. This file makes it.
//
// A probe (2026-07-11) laid candidate fire-tier peak temperatures against the
// reduction, roast and melt gates already tabulated in smelt.h. It set out to
// confirm the tier peaks reproduced history and instead turned up the thing worth
// keeping:
//
//   THE FIRE HAS TWO GATES, AND THEY ARE INDEPENDENT.
//
//   1. REDUCTANT. A reducing fire wins metal by stripping oxygen off an ore, and
//      the thing that does the stripping is CARBON carried in the charge. Raw wood
//      cannot do it. Wood is cellulose -- carbon already bound to oxygen and
//      hydrogen, already oxidised -- so burning it in air makes CO2 and reduces
//      nothing. Only CHARCOAL, wood with its oxygen and hydrogen driven out by
//      pyrolysis, is the near-pure carbon a smelt needs. This gate is CHEMICAL, not
//      thermal: the probe showed a bare stick fire at ~900 C clears the ~800 C
//      reduction onset and still cannot smelt, because it carries no reductant.
//      smelt.h already enforces exactly this -- reduction is gated on in_fuel[EL_C]
//      -- so a wood fire (zero carbon in the charge) reduces zero iron at any heat.
//
//   2. PEAK TEMPERATURE. Set by fuel x air, it decides not what REDUCES but what
//      MELTS. Open wood (~900 C) fires a clay pot and pours tin, and stops there.
//      Charcoal under natural draft (~1100 C) pours copper. Only charcoal under
//      forced air -- a bellows, a tuyere -- reaches the bloomery's reducing
//      envelope (~1200 C) and wins iron, which even then never melts (Tm 1538 C is
//      past every fire here, which is why iron is forged as a sponge, not cast).
//
// So charcoal is mandatory for reduction (gate 1) at ANY temperature, and forced
// air is mandatory for iron (gate 2). The two gates together are why fire.h fires a
// pot on open wood with no reductant at all, while the bloomery demands charcoal
// under a bellows: they are answering two different questions with one fire.
//
// Scope (design decisions, 2026-07-11): fuel is gathered items feeding authored
// fire TIERS, not a substance whose calorific value computes a temperature; the
// tree is a wood SOURCE with no season/regrowth clock. So this file authors the
// tier peaks and the char yield and does not pretend to derive them.

namespace wrought {

// ---------------------------------------------------------------------------
// Wood, as gathered.
//
// Split by HOW it is won, because that is the distinction the gather verb and the
// axe gate turn on -- not by species, which the model has no use for. Bare hands
// (the worst point on the curve, not "no tool") pull dead sticks from a standing
// tree and rake up needles; they cannot fell it. Felling for timber needs the axe,
// which needs a hafted edge, which needs worked wood -- the staircase this whole
// subsystem exists to close.

struct Wood {
    double tinder = 0.0;   // kg, fine dead litter -- pine needles, the example. Lights a fire; it is the match, not the fuel.
    double sticks = 0.0;   // kg, dead deadwood pulled from a standing tree. Hands.
    double timber = 0.0;   // kg, felled and split. Needs the axe. Haft stock and bulk fuel/char.
    double moisture = 0.0; // mass fraction. Green wood neither lights nor chars well.

    // What actually sustains a burn. Tinder starts it and is spent; the mass that
    // carries the fire is sticks and timber.
    double burnable() const { return sticks + timber; }
};

// A fire will not start without dry tinder: fine, dead, quick-catching litter, the
// needles under the pine in the finding's own example. AUTHORED gate (issue #32):
// the SIGN -- that ignition needs some dry tinder -- is the point, not the moisture
// threshold, which is a placeholder.
inline constexpr double TINDER_MOISTURE_MAX = 0.35;
inline bool can_light(const Wood& w) {
    return w.tinder > 0.0 && w.moisture < TINDER_MOISTURE_MAX;
}

// ---------------------------------------------------------------------------
// Pyrolysis: wood into charcoal.
//
// Charcoal is what a smelt consumes, and it already has a home in the model --
// CARBON is a phase in Substance -- so pyrolysis is a within-Substance transform,
// not a new kind of matter. (Wood itself is NOT in the phase table: a stick is not
// a Wentworth grain, and forcing it into a mineral size matrix would be a lie of
// convenience. Wood is its own stock above; only its PRODUCT crosses into
// Substance.) The rest of the wood mass -- oxygen, hydrogen, volatiles -- leaves;
// what remains is carbon, the reductant.

// Dry wood -> charcoal, by mass. Traditional pit/mound carbonisation. UNVERIFIED
// (a literature value exists: ~20-25% by mass for an earth kiln, higher for a
// sealed retort). Issue #30. Correcting it moves how much wood a smelt costs, not
// whether charcoal is required.
inline constexpr double CHAR_YIELD = 0.22;

// Char a mass of wood into a charcoal charge: a Substance whose CARBON is exactly
// what smelt.h reduces with. Moisture is driven off first and comes off the yield
// (a soft penalty, not a gate). Charcoal is lump, so it lands in GRAVEL; the smelt
// reads only its element mass, so the bin is bookkeeping.
inline Substance pyrolyze(double wood_kg, double moisture = 0.0) {
    Substance c;
    const double dry = wood_kg * (1.0 - moisture);
    c.freegrain[CARBON][GRAVEL] = (dry > 0.0 ? dry : 0.0) * CHAR_YIELD;
    return c;
}
inline Substance pyrolyze(const Wood& w) {
    return pyrolyze(w.burnable(), w.moisture);
}

// ---------------------------------------------------------------------------
// The fire, as a peak temperature.
//
// Gate 2. AUTHORED, cited envelope, UNVERIFIED here (issue #31). These are the
// numbers fire.h and smelt.h read to decide what a fire can do, and being authored
// they carry an obligation: they must reproduce the historical order. The probe
// checked them against smelt.h's own constants -- open wood below TM_COPPER
// (1357.8 K) so it cannot pour copper, above TM_TIN (505.1 K) so it pours tin;
// charcoal/draft above TM_COPPER so it pours copper, below the bloom envelope so it
// cannot make iron; charcoal/forced above the bloom envelope; every tier below
// TM_IRON (1811 K) so iron is never molten. The test asserts that ordering, so
// these numbers cannot drift out of history unnoticed.

enum FuelKind { WOOD, CHARCOAL };
enum Draft    { OPEN, NATURAL, FORCED };

// The bloomery's reducing envelope, ~1200 C. A comment in smelt.h, not a constant
// there (issue #21); named here because it is the gate the top fuel tier must clear
// and the two files should read one number for it if it ever moves.
inline constexpr double BLOOM_ENVELOPE_T = 1473.0; // ~1200 C. AUTHORED, issue #21/#31.

inline double peak_temperature(FuelKind fuel, Draft draft) {
    // Raw wood: an open bonfire, ~900 C. Forced air on raw wood is not modelled --
    // blast it and you are charring it, which is the CHARCOAL path, not a hotter
    // wood fire -- so wood ignores the draft.
    if (fuel == WOOD) return 1173.0;                  // ~900 C
    // Charcoal: natural draft ~1100 C, forced (bellows/tuyere) ~1300 C.
    return draft == FORCED ? 1573.0 : 1373.0;
}

} // namespace wrought
