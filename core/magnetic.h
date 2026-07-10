#pragma once

#include <cmath>
#include "separate.h"

// The lodestone. A found tool, never crafted (DESIGN.md, "the one tool the world
// gives you") -- naturally magnetised magnetite, the only magnetic separator that
// exists before metallurgy. You drag it through a panned concentrate and the
// black sand leaps to the stone.
//
// It reads exactly one column, `magnetic_susceptibility`, and separates on it by
// a force balance -- which is where DESIGN.md's fidelity ceiling says to derive:
// a grain leaps when the magnetic force on it beats its own weight.
//
//     F_mag / V = chi * B * grad(B) / mu0        (SI; chi volume susceptibility)
//     F_grav / V = rho * g
//
// so the grain lifts when the LIFT NUMBER
//
//     L = chi * (B grad B) / (mu0 * rho * g)   >  1.
//
// Two things fall out of that ratio and both matter.
//
// The grain VOLUME cancels -- exactly as u* and kappa cancelled out of the
// roughness bridge in fire.h. The lodestone has no opinion about how big a grain
// is, only about its chi/rho: its MASS susceptibility. And the whole magnet
// enters as the single product B*grad(B), never B or grad(B) alone, so a natural
// lodestone contributes exactly one number, not two.
//
// And that one number barely matters. Magnetite's chi/rho is ~3000x every other
// phase in the table (magnetite is ferromagnetic; hematite, ilmenite, goethite,
// pyrite are feebly paramagnetic; quartz, feldspar, clay, calcite, carbon are
// diamagnetic and actively repelled). So a magnet set anywhere from ~15x to
// ~1000x magnetite's threshold reach lifts ALL the magnetite and NONE of the
// rest. The output is therefore fixed by the feed's magnetite LIBERATION, not by
// any property of the stone -- which is the honest reading of "grade goes from
// ~70% to ~95% in one pass": the 95% is a statement about how well liberated the
// feed's magnetite is, and the magnet is only allowed to fail it, never to beat
// it. A hand lodestone and a laboratory magnet clean the same black sand to the
// same grade. The two authored magnet numbers below are asserted IRRELEVANT by a
// test, the same way roughness was shown unable to blur the pan past 3.4%.
//
// This is why the lodestone "does not cheat the law" (DESIGN.md): it is a
// separator on an axis orthogonal to the pan's, with a grade/recovery curve of
// its own, and a locked magnetite grain still drags its quartz to the stone and
// still caps the grade at the composite's. No pan can split magnetite from
// hematite -- they settle within 2% -- and the lodestone splits them trivially,
// because it is asking a different question.

namespace wrought {

inline constexpr double MU_0 = 4.0e-7 * M_PI; // T m / A, exact by definition
inline constexpr double G_ACCEL = 9.80665;    // m/s^2, standard gravity

// B * grad(B) of a vigorous natural lodestone. Surface fields of natural
// lodestone run ~1-100 mT and the near-field gradient ~10-100 T/m, so the product
// spans ~0.01-10 T^2/m. AUTHORED at 0.9 -- about 40x magnetite's 0.0212 T^2/m
// threshold, mid-band -- and its exact value is asserted not to matter. Issue:
// authored-number (#20).
inline constexpr double MAGNET_REACH = 0.9; // T^2/m

// Field non-uniformity smears the lift threshold into a logistic, the same shape
// the pan uses on velocity. AUTHORED, and asserted not to matter either, because
// the chi/rho gap between magnetite and everything else dwarfs it. Issue:
// authored-number (#20).
inline constexpr double LODESTONE_SHARPNESS = 0.5; // sigma in ln(L)

// Volume-mixed susceptibility of a composite grain (target phase p plus gangue).
// Same idiom as composite_density: mix by volume fraction, not mass.
inline double composite_susceptibility(int p) {
    constexpr double f = COMPOSITE_TARGET_FRACTION;
    const double vp = f / PHASES[p].density;
    const double vg = (1.0 - f) / PHASES[GANGUE].density;
    return (vp * PHASES[p].magnetic_susceptibility
          + vg * PHASES[GANGUE].magnetic_susceptibility) / (vp + vg);
}

// Magnetic force over weight. density in g/cm^3, the phase table's own units.
// A diamagnetic grain (chi < 0) is pushed away, never lifted: return 0.
inline double lift_number(double chi, double density_gcc, double reach = MAGNET_REACH) {
    if (chi <= 0.0) return 0.0;
    return chi * reach / (MU_0 * density_gcc * 1000.0 * G_ACCEL);
}

// Probability a grain reports to the stone. Logistic in ln(L), cut at L = 1.
inline double magnetic_partition(double L, double sigma = LODESTONE_SHARPNESS) {
    if (L <= 0.0) return 0.0;
    return 1.0 / (1.0 + std::exp(-std::log(L) / sigma));
}

// Drag the lodestone through a substance. The concentrate is what leapt to the
// stone; the tailings are what it left behind. Free grains report at their own
// chi/rho; a composite reports at the volume-mixed chi/rho of grain-plus-gangue,
// and brings its gangue with it -- which is where the grade ceiling comes from,
// exactly as in separate().
inline SeparationResult lodestone(const Substance& in,
                                  double reach = MAGNET_REACH,
                                  double sigma = LODESTONE_SHARPNESS) {
    SeparationResult out;
    out.concentrate.temperature = out.tailings.temperature = in.temperature;
    for (int p = 0; p < N_PHASE; ++p) {
        const double wf = magnetic_partition(
            lift_number(PHASES[p].magnetic_susceptibility, PHASES[p].density, reach), sigma);
        const double wc = magnetic_partition(
            lift_number(composite_susceptibility(p), composite_density(p), reach), sigma);
        for (int s = 0; s < N_SIZE; ++s) {
            out.concentrate.freegrain[p][s] = in.freegrain[p][s] * wf;
            out.tailings.freegrain[p][s]    = in.freegrain[p][s] * (1.0 - wf);
            out.concentrate.composite[p][s] = in.composite[p][s] * wc;
            out.tailings.composite[p][s]    = in.composite[p][s] * (1.0 - wc);
        }
    }
    return out;
}

} // namespace wrought
