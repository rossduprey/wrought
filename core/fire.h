#pragma once

#include <cmath>
#include "levigate.h"

// Clay into pot. This is the file where the ratchet is supposed to live, and
// therefore the file to attack first: it holds every authored bridge in Era 1,
// and there are two.
//
// DESIGN.md states the ratchet like this:
//
//   > How finely you levigate your clay sets how densely your pan fires. How
//   > dense your pan is sets its grade/recovery curve. That curve sets the purity
//   > of your next concentrate -- including your next batch of clay.
//
// The first clause is true and is ceramics. The **second clause is the one to
// doubt**, and this file exists to make it falsifiable rather than repeat it.

namespace wrought {

// ---------------------------------------------------------------------------
// What a fired body is made of.

inline double clay_grade(const Substance& body) { return body.grade(KAOLINITE); }

// The size of the coarsest thing embedded in the fired surface, mass-weighted
// over everything that is not a clay mineral. Kaolinite vitrifies; the rest sits
// in the fabric as grit and stands proud of it. Derived, not authored.
inline double grit_diameter(const Substance& body) {
    double m = 0.0, md = 0.0;
    for (int p = 0; p < N_PHASE; ++p) {
        if (p == KAOLINITE) continue;
        for (int s = 0; s < N_SIZE; ++s) {
            const double cell = body.freegrain[p][s] + body.composite[p][s];
            m += cell;
            md += cell * bin_diameter(s);
        }
    }
    return m > 0.0 ? md / m : 0.0;
}

// ---------------------------------------------------------------------------
// BRIDGE 1 — grit into sharpness. The claim DESIGN.md makes.
//
// A pan's cut is the shear velocity of the water over its floor. A rough floor
// makes that velocity vary from place to place, so the pan does not have one cut,
// it has a distribution of them, and the partition it *reports* is the average of
// many logistics with jittered centres -- which is a wider logistic. Variances
// add. That much was right the first time and it is still right.
//
// Everything after it was wrong. This file used to say:
//
//     sigma_pan^2 = sigma_wrist^2 + (d_grit / h_skin)^2                  [WRONG]
//
//   > "The quadrature is derivable. The identification of the ln-velocity spread
//   > with `d_grit/h_skin` is AUTHORED: it says the velocity grows roughly
//   > linearly across the skin, which is true of a viscous sublayer and only
//   > roughly true here. `h_skin` is not new -- it is the same skin the bed
//   > already needed, so this bridge introduces no constant of its own."
//
// Issue #10 said that conclusion was robust to the bridge's *magnitude* but not
// to its *functional form*, and feared a form like sqrt(d/h), which would have
// moved every number by a factor of 35 and might have restored the ratchet. So
// somebody went and derived it. *(2026-07-10, #10. What follows is the derivation;
// the wrong line above stays, per CONTRIBUTING.md, because without it somebody
// will helpfully revert this.)*
//
// **The spread.** Over a rough bed the velocity profile is the log law,
//
//     u(z) = (u* / kappa) * ln(z / z0),        z0 = k_s / 30   (Nikuradse)
//
// with k_s the roughness height -- here the grit diameter. A grain of diameter
// d_cut resting on that bed has its centre at z_c = d_cut/2 + zeta, where zeta is
// how high the local bed surface happens to sit: a grain can rest on a crest or in
// a hollow. The drag it feels goes as u(z_c)^2, so its cut velocity is
// proportional to u(z_c), and the spread the partition sees is the spread of
//
//     ln u(z_c)  =  const + ln( ln(z_c / z0) ).
//
// To first order in zeta, with sd(zeta) = c_bed * d_grit,
//
//     j  =  2 * c_bed * (d_grit/d_cut) / ln( 15 * d_cut/d_grit ).
//
// Three things about that expression are worth more than the number it produces.
//
//   * **kappa and u* are gone.** They cancel: the spread of ln u does not depend
//     on how hard you wash, only on where the grain sits. The bridge inherits no
//     constant from the flow.
//   * **The denominator is d_cut, not h_skin.** What roughness jitters is the
//     elevation of the grain the separator is *deciding about* -- the one at v50,
//     whose partition value sigma actually controls. Grains far from the cut
//     report 0 or 1 whatever sigma is. The depth of the moving water never enters.
//     The old form got the right order of magnitude because `h_skin` happens to be
//     `3 * bin_diameter(SAND)` and the sand bin happens to sit near the cut, so an
//     authored 3 was standing in for a derivation. That is the whole story of it.
//   * **The exponent is 1 + 1/ln(15*d_cut/d_grit)**, which is 1.11 at the grit a
//     decant can deliver -- slightly *super*-linear. #10 feared 0.5. The blur dies
//     off faster than linearly as the grit gets fine, not slower.
//
// **The quadrature.** A logistic is a probit in disguise, and mixing a probit over
// Gaussian jitter in its centre gives a probit of the summed variance -- but this
// project reports sigma through *quartiles* (9^sigma), not through variance, and
// the two do not agree. Expanding the mixed logistic to second order in j,
//
//     x_75  =  sigma*ln3 + j^2/(4*sigma)   =>   sigma' = sigma + j^2/(4*sigma*ln3)
//
// so what adds in quadrature is not j but j/sqrt(2*ln3). The old line added j
// itself, which overstated the blur by a further 1.48x. Checked against a numeric
// mixture in the test suite, which returns 0.6744 against 1/sqrt(2 ln 3) = 0.6746.
//
//     sigma_pan^2 = sigma_wrist^2 + (QUADRATURE * j)^2
//
// **What survives being authored.** One number: `c_bed`, the standard deviation of
// the bed's surface elevation in units of the grit diameter. It is 1/sqrt(12) for
// a uniform monolayer and no plausible packing puts it outside 0.25-0.35, so the
// bridge is bounded where it used to be invented. The Nikuradse 30 is cited. The
// quadrature is derived. `d_cut` is derived by inverting the force balance at the
// cut velocity. Issue #18, and it is a small one now: everything left in it moves
// the blur by a factor of order one, and the blur is 9.4e-5 against a wrist of 0.55.
//
// **Net.** At the coarsest grit a decant can carry (0.883 um, clay-sized), the old
// bridge said 8.33e-4 and the derivation says 9.40e-5. The old form was 8.85x too
// big, from three errors that partly cancelled: the wrong denominator made it 1.73x
// too small, the missing logarithm made it 8.85x too big, and the missing
// quadrature coefficient made it 1.48x too big. **The staircase stands, with 8.85x
// more margin than it had.** #10 asked whether the form could restore the spiral.
// It cannot; it buries it deeper.
//
// ---------------------------------------------------------------------------
// A separator has two misplacements, not one. AGAIN.
//
// `separate.h` learned on 2026-07-09 that a screen reports its coarse recovery and
// its fine misplacement independently, and that squeezing both into one `efficiency`
// made hand-picking pebbles cost 15% of your sand. The same mistake was in this
// file, wearing different clothes, and nobody noticed for a day.
//
// When the grit is coarser than the grain you are sorting, the grain stops resting
// *on* the roughness and starts hiding *between* the elements of it. Its centre
// falls below z0, the log law hands it no flow at all, and it does not move -- at
// any cut, however hard you wash. That is not a blur. It is a grain that never
// entered the separation, and the right name for it is shelter.
//
// The old bridge had no shelter, so it charged the whole cost of a stony pot to
// sigma and got sigma = 1.83 for a pot pinched from raw dirt: worse than cupped
// hands, which a test asserted and which is *false*. The exposed sand on a cobbled
// floor is sorted about as well as on a smooth one. The 42% of it wedged between
// the stones is sorted not at all.
//
// So `roughness_spread` is valid only for d_grit < d_cut, and it is CLAMPED at that
// edge and returns a LOWER BOUND beyond it. Above the edge, ask `sheltered_fraction`
// instead; the honest statement about a stony pot is not that it blurs, but that it
// swallows.
//
// And now it swallows in the arithmetic, not only in the prose (#19, 2026-07-10).
// `fire_pan` reads `sheltered_fraction` at every grain size into the separator's
// per-bin `shelter[]`, and `separate.h` sends that share of each bin straight to
// the concentrate: a wedged grain does not move, so it stays in the pan whatever
// its velocity. It is a SECOND misplacement, carried independently of the blur,
// exactly as `screen()` carries `coarse_to_oversize` and `fine_to_oversize`. The
// hiding-exposure function the issue reached for (Egiazaroff 1965, Wilcock & Crowe
// 2003) is empirical and would have imported fitted constants; the geometry
// already derived here -- the grain's centre below z0 -- IS a hiding-exposure
// function, and evaluating it per size bin closes #19 with no new authored number.
// The effect is the one the issue predicted: a stony pan's concentrate is diluted
// toward feed grade by fines it should have rejected but cannot wash out.
//
// The consequence is a claim stronger than the one it replaces: within its domain
// **roughness cannot blur the pan by more than 3.4%.** The spread tops out at 0.213
// when the grit is as coarse as the grain, sigma_pan tops out at 0.5685, and the
// 10% blur that #10 priced at 267 um of grit is not purchasable at any grit at all
// -- what a stony floor costs is recovery to shelter, never grade to blur.

inline double WRIST_SHARPNESS = 0.55;  // AUTHORED. The blur of a human wrist,
                                       // and the irreducible floor of any pan.
                                       // It is PAN.sharpness, renamed for what
                                       // it is: a property of the operator, not
                                       // of the pot.

// Nikuradse (1933): a bed of packed grains of size k_s has the log law's velocity
// vanishing at z0 = k_s/30. Cited, standard, and the only literature constant here.
inline constexpr double NIKURADSE = 30.0;

// The sd of the bed's surface elevation, in grit diameters. 1/sqrt(12) is the
// uniform monolayer. AUTHORED -- and it is the only authored number in the bridge.
inline constexpr double BED_ELEVATION_SD = 0.28867513459481288;

// What multiplies the jitter in the quadrature. DERIVED: 1/sqrt(2*ln 3), because
// sigma is reported at the quartiles. Not 1, which is what this file used to use.
inline const double QUADRATURE = 1.0 / std::sqrt(2.0 * std::log(3.0));

// The grain the pan is actually deciding about: quartz falling at exactly the cut.
// Inverting the force balance introduces no number -- the density is the phase
// table's and the velocity is the operator's dial. Quartz because the pan is mostly
// full of it and the bed the water runs over is quartz sand; a magnetite grain at
// the same cut is ~1.4x finer, which moves the blur by 1.4x and the blur is 1e-4.
inline double cut_diameter(double v_cut, double kelvin = REFERENCE_TEMPERATURE) {
    if (v_cut <= 0.0) return 0.0;
    double lo = 1e-9, hi = 0.2;   // 1 nm to 20 cm brackets anything a pan holds
    for (int i = 0; i < 80; ++i) {
        const double mid = std::sqrt(lo * hi);   // bisect in ln d; v is monotone in d
        if (settling_velocity(PHASES[QUARTZ].density, mid, 1.0, kelvin) < v_cut) lo = mid;
        else hi = mid;
    }
    return std::sqrt(lo * hi);
}

// The spread in ln(cut velocity) that a floor of `d_grit` imposes on a grain of
// `d_cut`. Clamped at d_grit = d_cut; beyond that see `sheltered_fraction`.
inline double roughness_spread(double d_grit, double d_cut) {
    if (d_grit <= 0.0 || d_cut <= 0.0) return 0.0;
    const double r = std::fmin(d_grit / d_cut, 1.0);
    return 2.0 * BED_ELEVATION_SD * r / std::log(0.5 * NIKURADSE / r);
}

// The fraction of grains of diameter `d_grain` whose centres sit below z0 on a
// floor of grit `d_grit` -- wedged between the grit, feeling no flow, leaving the
// separation before it starts. It is a function of the ratio, so it answers for a
// whole size bin (pass `bin_diameter(s)`) as readily as for the cut grain (pass
// `d_cut`). Zero for a grain as coarse as its floor; 0.53 for one far finer. This
// is now APPLIED, not merely reported: `fire_pan` reads it into the separator's
// per-bin `shelter[]`, and `separate()` sends that share straight to the
// concentrate (#19). Zero for any pot thrown from levigated clay; ~0.42 at the cut
// for a pot pinched from raw dirt.
inline double sheltered_fraction(double d_grit, double d_grain) {
    if (d_grit <= 0.0) return 0.0;
    return std::fmax(0.0, std::fmin(1.0, 0.5 + 1.0 / NIKURADSE - 0.5 * d_grain / d_grit));
}

inline SeparatorParams fire_pan(const Substance& body) {
    const double d_grit = grit_diameter(body);
    const double blur = QUADRATURE *
        roughness_spread(d_grit, cut_diameter(PAN.cut_velocity));
    SeparatorParams sp{PAN.cut_velocity, std::hypot(WRIST_SHARPNESS, blur), "fired pan"};
    // The second misplacement, filled per size bin: a grain shelters against the
    // floor's grit at its own diameter, not the cut's. A smooth (levigated) floor
    // leaves every entry zero, and separate() then behaves exactly as before.
    for (int s = 0; s < N_SIZE; ++s)
        sp.shelter[s] = sheltered_fraction(d_grit, bin_diameter(s));
    return sp;
}

// ---------------------------------------------------------------------------
// BRIDGE 2 — clay into vessel. The claim DESIGN.md does not make, and which
// turns out to be the one that carries the loop.
//
// A green body stands up because clay platelets are plastic and grit is not.
// Below some clay fraction a pinch pot will not hold its own wall; at pure clay
// it stands to some height and no higher, because a wet wall creeps. So the
// tallest wall you can throw scales with the clay fraction of your body, and the
// pot's volume with the cube of that -- a pot being a shape, not a wall.
//
// MAX_WALL is AUTHORED and UNVERIFIED. The linearity is authored too, and it is
// authored because we have no data, not because we have a reason. Both should be
// a tracked issue. Everything downstream of them is geometry.
inline constexpr double MAX_WALL = 0.30;  // m, a pure-clay pinch pot. UNVERIFIED.

inline Vessel throw_pot(const Substance& body) {
    const double h = MAX_WALL * clay_grade(body);
    return Vessel{h, 0.5 * h, "a fired pot"};   // a bowl twice as wide as it is deep
}

} // namespace wrought
