#pragma once

#include "fuel.h"
#include "firekit.h"

// The charcoal pit: the station where wood becomes the reductant, and the one
// gate that station turns on.
//
// fuel.h established WHAT charcoal is and WHY a smelt needs it (pyrolyze(), the
// two-gate finding). It left pyrolyze() as the IDEAL transform: dry wood in, its
// carbon out at CHAR_YIELD. This file is the STATION that runs that transform in
// the real world -- a pit or a mound -- and the real world adds one thing the
// ideal did not have, which is the whole reason the station is a station and not
// a line of arithmetic:
//
//   THE PIT'S TRICK IS STARVING THE FIRE THAT MAKES THE CHAR.
//
//   Pyrolysis needs heat, and the only heat here comes from burning wood. So the
//   same fire that drives the volatiles out of the charge will, given air, go on
//   to burn the CHARCOAL too -- carbon plus oxygen is CO2, the char oxidised right
//   back to gas and ash. A charcoal pit is therefore not a fire you light, it is a
//   fire you SUFFOCATE: bank the wood, cover it, and choke the draft down to the
//   sliver that carbonises the charge without consuming it. Open that same pit to
//   the air and you have a bonfire, which leaves ash, not charcoal. The char you
//   carry to the furnace is not what the wood HELD -- it is what you KEPT from the
//   fire, and the seal is what decides how much.
//
// This is the gate the free charcoal_bed() in furnace.cpp never had: there,
// carbon was laid beside the ore by fiat. Here it is made, and making it can fail.
//
// Scope (design decision, 2026-07-11): fuel is gathered items feeding authored
// stations, not a combustion model. So the pit's seal is an authored gate with a
// few discrete settings, not a computed air/fuel burn. The SIGN is the finding --
// more air, less char, to nearly none wide open -- not the retained fractions.

namespace wrought {

// How well the pit is sealed against the air. The one decision the station makes.
// PIT_OPEN, not OPEN: fuel.h's Draft already owns OPEN in this namespace.
enum PitSeal {
    PIT_SEALED,   // covered mound / capped pit: air choked to the carbonising sliver.
    PIT_VENTED,   // leaky cover: some charge burns through to ash before it is drawn.
    PIT_OPEN      // no cover -- a bonfire. Leaves ash; a char pit only by accident.
};

// Fraction of the ideal (fully-sealed) char yield that SURVIVES the burn at each
// seal. AUTHORED placeholders (issue #33): a fully sealed pit keeps all the char
// pyrolysis would give (1.0); air eats into it, to nearly nothing wide open. The
// ordering SEALED > VENTED >> OPEN is the finding and is asserted; the numbers
// between are stand-ins for a combustion model this project chose not to build.
inline double seal_retention(PitSeal seal) {
    switch (seal) {
        case PIT_SEALED: return 1.00;
        case PIT_VENTED: return 0.50;
        case PIT_OPEN:   return 0.05;
    }
    return 0.0;
}

// Run the pit: bank a mass of wood, choke it to the given seal, draw the char.
// The output is a charcoal charge -- a Substance whose CARBON is exactly what
// smelt.h reduces with -- scaled from the ideal pyrolysis yield by what the seal
// let survive. A SEALED pit reproduces pyrolyze() exactly; anything looser loses
// carbon to the fire before you can rake it out.
inline Substance char_pit(double wood_kg, double moisture, PitSeal seal) {
    Substance c = pyrolyze(wood_kg, moisture);
    const double keep = seal_retention(seal);
    c.freegrain[CARBON][GRAVEL] *= keep;
    return c;
}
inline Substance char_pit(const Wood& w, PitSeal seal) {
    return char_pit(w.burnable(), w.moisture, seal);
}

// A pit is a FIRE before it is a kiln. The two overloads above are the ideal
// transform -- they assume the pit is already burning. But nothing burns itself:
// firekit.h makes the spark a MADE tool, and a pit banked with no smolder kit (or no
// dry tinder to catch) never lights, so unlit wood chars to nothing. This overload
// is the honest station: bank a FireKit, strike it with the smolder tool, and only a
// pit that CATCHES yields char -- scaled, as ever, by the seal. The tinder rung is
// spent lighting it; the burnable mass (sticks and any timber) is the charge.
inline Substance char_pit(const FireKit& kit, const SmolderKit& spark, PitSeal seal) {
    if (!ignite(kit, spark).lit) return Substance{};   // no spark, no fire, no char.
    return char_pit(kit.burnable(), kit.moisture, seal);
}

} // namespace wrought
