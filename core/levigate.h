#pragma once

#include <cmath>
#include "separate.h"

// Levigation — separation by settling time, in a vessel.
//
// This is the second separator in the project and it shares nothing with the
// first but the settling velocity it reads. `separate()` is a logistic in ln(v)
// with an authored sharpness. Levigation has **no sharpness to author**, because
// a batch decant's partition function can be written down exactly:
//
//   Fill a vessel to depth h. Stir. Every grain is now uniform in depth. Wait t.
//   A species falling at v has descended v*t, so it occupies [0, h - v*t] at its
//   original concentration, and the water above that front is clear of it. Pour
//   off everything above the sediment. The mass you carry away is the overlap.
//
//     poured(v) = clamp( (clear - v*t) / h, 0, 1 )
//
// That is a *ramp* in v, not a sigmoid, and it terminates: a species with
// v >= clear/t is rejected completely, not merely mostly. Its imperfection —
// the quartile ratio the rest of this project reports — is exactly 3.0, always,
// for every vessel, every charge and every wait. There is no dial.
//
// Three consequences, none of them designed. **Two of the three were wrong**, and
// they were wrong for one reason two files away: `settling.h` gave kaolinite the
// drag of a marble. Issue #13 gave it the drag of a plate on 2026-07-10, and what
// it changed was not this file's algebra -- which never moved -- but every number
// the algebra was standing on.
//
//   * **Patience buys grade. It cannot buy recovery.** The cut is clear/t and t
//     is free, so a long enough wait rejects anything you like. What a long wait
//     costs you is the clay itself, which is not weightless and eventually lands.
//
//     This one survived, and it is now worth much more than it was: patience buys
//     grade all the way to **1.000**, because there is something left to reject.
//
//   * **The pot buys recovery, and nothing else buys it.** You can only pour off
//     the water above the sediment, and the sediment's thickness is the charge
//     divided by the vessel's cross-section. For a grain light enough never to
//     land, the pourable fraction is 1 - m_settled/(rho_bulk * V): it depends on
//     the vessel's **volume** alone, and depth cancels out of it exactly.
//
//     *(That last clause is as far as the algebra goes, and this comment claimed
//     more than that for about an hour. Clay is not weightless. Its own front
//     falls v*t, and that distance is measured against the depth, so a deep
//     narrow jar beats a shallow wide pan of the same volume -- 19.5% recovery
//     against 4.8%, at a matched four-hour wait. Volume sets what you may pour;
//     depth sets how much of your clay is still up there when you pour it. It
//     took the program printing both to notice.)*
//
//     *(And the algebra was right the first time. A kaolinite PLATELET's front
//     falls 1.3 mm in four hours, not 8.6 mm, so against a 50 mm hollow it is
//     negligible and depth cancels as the algebra always said it did: 3.0x
//     collapses to 1.12x. The correction above was published to cover for a
//     defect in `settling.h`, and it is the only entry in this project's record
//     of its own errors that is itself an error. It stays. -- 2026-07-10, #13.)*
//
//   * **Re-decanting the liquor is a tax, not a process.** The first pour throws
//     away everything that settles. What is left is one velocity class, and no
//     velocity separator can divide a single velocity class -- so the second
//     decant returns the feed, minus a slice. Measured: +0.0001 grade and -4.5%
//     recovery, per pass, forever.
//
//     This is Era 0's own law wearing different clothes. The test suite once had
//     an assertion that "even the gentlest pan enriches", and it was deleted for
//     being false in exactly this way. Two separators, two eras, one mistake,
//     and the second time it was caught by a failing test rather than a passing
//     one. (I had written "the recovery falls off a cliff" in this comment. It
//     does not. It fell off a cliff in a scratch probe that held the pour
//     fraction at a constant 0.80 instead of deriving it from the sediment.)
//
//     *(Era 0's law is sound and it was applied to a false premise. The clay bin
//     is NOT one velocity class: clay-sized quartz falls 6.98x faster than a
//     clay-sized kaolinite platelet. It was one velocity class only while both
//     were spheres of one diameter, at which point nothing distinguished them but
//     a 1.9% difference in density. Re-decanting is a cheap process: +0.013 grade
//     for -2.1% recovery over three passes. The paragraph above is a correct
//     argument from a wrong fact, which is the most expensive kind, and it stood
//     for a day. -- 2026-07-10, #13.)*
//
// Not modelled: hindered settling (a dense suspension settles slower than a
// dilute one, Richardson-Zaki), and flocculation (a real potter deflocculates to
// hold clay up and flocculates to drop it, and a floc falls as one big grain).
// Both are absent. Both would make levigation easier than it is here.
//
// Deflocculation was Era 1's missing lodestone while levigation could not divide
// the clay bin (#15). It can. Deflocculation is now an **improvement** rather
// than a **requirement**: the hollow reaches pure kaolinite in 16 hours at 10.6%
// recovery, and the large pot in eleven days at 43.5%. Holding the clay up by
// surface charge is what buys you both at once. That is a better set piece than
// the one it replaces, because the player has a working process to improve rather
// than a wall to be handed a key for.

namespace wrought {

// A cylinder of water. Only the volume turns out to matter, but a vessel that
// carried only a volume would let you dig a hollow one grain deep and a hectare
// across, and the clay would blow away.
struct Vessel {
    double depth;   // m
    double radius;  // m
    const char* name;

    double area()   const { return M_PI * radius * radius; }
    double volume() const { return area() * depth; }
};

// The world's gift, like the lodestone: a scrape in the bank, holding water
// because the bank is clay. You dug it with your hands, so it is wide and it is
// shallow, and the sediment fills most of it. AUTHORED, and it is the *only*
// authored vessel -- every later one is thrown from clay this one produced.
inline constexpr Vessel HOLLOW = {0.05, 0.20, "a hollow scraped in the bank"};

// The fraction of a species that has reached the floor after t seconds.
inline double settled_fraction(double v, double depth, double t) {
    return std::fmin(1.0, v * t / depth);
}

// How deep the settled solids lie. Reuses the packed-wet-sand density the bed
// already needed; a sediment and a bed are the same object seen from two sides.
inline double sediment_thickness(const Substance& mud, const Vessel& ves, double t) {
    const double T = mud.temperature;
    double settled = 0.0;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            settled += mud.freegrain[p][s] * settled_fraction(free_velocity(p, s, T), ves.depth, t);
            settled += mud.composite[p][s] * settled_fraction(composite_velocity(p, s, T), ves.depth, t);
        }
    return settled / (BULK_DENSITY * ves.area());
}

// The clear water you can actually tip off, in metres. Negative means you filled
// the vessel with more dirt than it can hold and there is nothing above it.
inline double clear_depth(const Substance& mud, const Vessel& ves, double t) {
    return ves.depth - sediment_thickness(mud, ves, t);
}

// The velocity above which a grain is rejected outright. This is the operator's
// cut, and unlike a pan's it is not a wash strength -- it is how long he waited.
inline double levigation_cut(double clear, double t) {
    return (t > 0.0 && clear > 0.0) ? clear / t : 0.0;
}

inline double poured_fraction(double v, double clear, double depth, double t) {
    if (clear <= 0.0) return 0.0;
    return std::fmax(0.0, std::fmin(1.0, (clear - v * t) / depth));
}

struct DecantResult {
    Substance liquor;    // what you poured off. This is your clay.
    Substance sediment;  // what stayed. Sand, silt, and the clay you were too slow for.
};

inline DecantResult decant(const Substance& mud, const Vessel& ves, double seconds) {
    DecantResult out;
    const double T = mud.temperature;
    out.liquor.temperature = out.sediment.temperature = T;
    const double clear = clear_depth(mud, ves, seconds);
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double pf = poured_fraction(free_velocity(p, s, T), clear, ves.depth, seconds);
            const double pc = poured_fraction(composite_velocity(p, s, T), clear, ves.depth, seconds);
            out.liquor.freegrain[p][s]   = mud.freegrain[p][s] * pf;
            out.sediment.freegrain[p][s] = mud.freegrain[p][s] * (1.0 - pf);
            out.liquor.composite[p][s]   = mud.composite[p][s] * pc;
            out.sediment.composite[p][s] = mud.composite[p][s] * (1.0 - pc);
        }
    return out;
}

// ---------------------------------------------------------------------------
// Deflocculation -- Era 1's real tool, and the second variable orthogonal to
// settling velocity (the lodestone was the first).
//
// Everything above this line models the DEFLOCCULATED state without ever naming
// it: a clay is bare platelets, each falling at its own velocity. That is what a
// suspension looks like once an electrolyte -- wood ash, or urine -- has charged
// its platelets so they repel and refuse to stack. Its opposite is FLOCCULATION:
// add a salt or an acid, the charge is killed, the platelets aggregate, and the
// clay falls as one big floc. Surface charge, not size, decides which -- a variable
// a velocity separator cannot read, the same shape of answer the lodestone is for
// magnetite and hematite, and the one issue #15 asked for.
//
// A probe on 2026-07-10 decided which half of this is honest to model. The romantic
// half -- "deflocculated clay stays up forever" -- is NOT single-particle Brownian
// suspension: at a clay platelet's size the Peclet number over a pottery vessel is
// ~1e4, so gravity beats diffusion by four orders of magnitude, and what really
// holds a deflocculated sol up is a many-body colloidal osmotic pressure (Hamaker
// constant, Debye length, zeta potential, volume fraction) -- the "six invented
// numbers" #15 itself warns is the wrong model. So we do NOT model indefinite
// suspension. We model the FLOCCULATION half, where the physics is clean: a floc is
// a grain of an AUTHORED effective size (#29) and falls by the same force balance as
// any other. Two wins fall straight out of it, and neither needs a curve:
//
//   1. RECOVERY ON DEMAND. A single platelet takes ~158 h to cross a hollow; a floc
//      crosses it in ~73 s. Deflocculate to pour a clean clay liquor, then flocculate
//      that liquor to get the clay BACK in minutes instead of days. Levigation gave
//      you grade for the price of patience; flocculation gives you the clay itself.
//
//   2. A SEPARATION VELOCITY CANNOT MAKE. Only clay minerals floc; quartz and
//      feldspar have no charge to kill and stay up. So flocculating a clay liquor
//      drops the kaolinite and leaves clay-sized quartz behind -- it divides a single
//      velocity class by surface charge. #13 taught velocity to split that class by
//      SHAPE (6.98x plate drag); this splits it by a truly orthogonal variable, and
//      the recovered solid comes out purer than the feed.

// The effective size a kaolinite floc falls at once its charge is killed. AUTHORED,
// UNVERIFIED (#29): a real floc is an open aggregate of tens of microns; both
// findings ride on this being MUCH coarser than a platelet, not on its value. Only
// clay minerals floc -- that selectivity is the whole point.
inline constexpr double FLOC_DIAMETER = 30.0e-6;  // m. AUTHORED, UNVERIFIED (#29).

// The velocity a grain falls at in a flocculated suspension. A clay-mineral fine
// becomes a floc of FLOC_DIAMETER (equant, so aspect 1.0); anything already coarser
// than a floc, and anything that does not floc, falls at its own velocity unchanged.
inline double flocculated_velocity(int p, int s, double T) {
    if (p == KAOLINITE && bin_diameter(s) < FLOC_DIAMETER)
        return settling_velocity(PHASES[KAOLINITE].density, FLOC_DIAMETER, 1.0, T);
    return free_velocity(p, s, T);
}

struct FloccResult {
    Substance recovered;    // reached the floor: your clay, dropped on demand
    Substance supernatant;  // still up: the water, and the fines that do not floc
};

// Flocculate a (deflocculated) clay suspension and let it stand `seconds`. The clay
// aggregates and drops as flocs; clay-sized quartz keeps its charge and stays up. So
// this recovers the clay FAST and divides it from quartz by surface charge -- the
// recovery half of the set piece, and the half whose physics needs no invented curve.
inline FloccResult flocculate(const Substance& susp, const Vessel& ves, double seconds) {
    FloccResult out;
    const double T = susp.temperature;
    out.recovered.temperature = out.supernatant.temperature = T;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double ff = settled_fraction(flocculated_velocity(p, s, T), ves.depth, seconds);
            out.recovered.freegrain[p][s]   = susp.freegrain[p][s] * ff;
            out.supernatant.freegrain[p][s] = susp.freegrain[p][s] * (1.0 - ff);
            // A composite is a locked grain, not a dispersed platelet: it does not
            // floc, it settles at its own velocity.
            const double fc = settled_fraction(composite_velocity(p, s, T), ves.depth, seconds);
            out.recovered.composite[p][s]   = susp.composite[p][s] * fc;
            out.supernatant.composite[p][s] = susp.composite[p][s] * (1.0 - fc);
        }
    return out;
}

} // namespace wrought
