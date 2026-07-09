#pragma once

#include <cmath>
#include "substance.h"

// Density separation as a partition function.
//
// DESIGN.md describes progression as a grade/recovery curve that a better tool
// "moves outward". A curve you author is a curve you balance, which the source
// rule forbids. So nothing here authors a curve: a separator is the probability
// that a particle reports to the concentrate, as a function of that particle's
// density. The grade/recovery curve is an *output* of two numbers.
//
//   cut_density  — the operator's call. Pan hard, raise the cut.
//   sharpness    — the tool. A duller tool blurs the cut, and no amount of skill
//                  repairs it. This is what "the tool moves the curve" means.
//
// This is the partition (Tromp) curve of mineral processing, in the one form
// where the tradeoff is arithmetic rather than a designer's opinion.

namespace wrought {

struct SeparatorParams {
    double cut_density;              // g/cm^3, rho50
    double sharpness;                // g/cm^3, smaller is a sharper cut
    double size_efficiency[N_SIZE];  // fines leave over the lip whatever they weigh
    const char* name;
};

// Cupped hands, then a low-fired pinch pot, then a built sluice. One function.
inline constexpr SeparatorParams HANDS  = {3.5, 1.60, {0.05, 0.55, 0.70}, "hands"};
inline constexpr SeparatorParams PAN    = {3.5, 0.80, {0.15, 1.00, 0.80}, "pan"};
inline constexpr SeparatorParams SLUICE = {3.5, 0.35, {0.35, 1.00, 0.95}, "sluice"};

struct SeparationResult {
    Substance concentrate;
    Substance tailings;
};

inline double partition(double particle_density, const SeparatorParams& sp, int size) {
    const double x = (particle_density - sp.cut_density) / sp.sharpness;
    return sp.size_efficiency[size] / (1.0 + std::exp(-x));
}

inline SeparationResult separate(const Substance& in, const SeparatorParams& sp) {
    SeparationResult out;
    out.concentrate.temperature = out.tailings.temperature = in.temperature;

    for (int p = 0; p < N_PHASE; ++p) {
        const double rho_free = PHASES[p].density;
        const double rho_comp = composite_density(p);

        for (int s = 0; s < N_SIZE; ++s) {
            const double cf = in.freegrain[p][s] * partition(rho_free, sp, s);
            out.concentrate.freegrain[p][s] = cf;
            out.tailings.freegrain[p][s]    = in.freegrain[p][s] - cf;

            // A composite reports as one particle, at one density, and its gangue
            // is not consulted. This is where the grade ceiling comes from.
            const double cc = in.composite[p][s] * partition(rho_comp, sp, s);
            out.concentrate.composite[p][s] = cc;
            out.tailings.composite[p][s]    = in.composite[p][s] - cc;
        }
    }
    return out;
}

// A screen is a separation by size and nothing else. It is the verb that makes
// crushing worth doing, and Era 0 in DESIGN.md did not have it.
// `efficiency` is the fraction of each particle that reports where it belongs;
// the remainder is misplaced. AUTHORED, UNVERIFIED. A perfect screen is 1.0 and
// does not exist.
struct ScreenResult { Substance oversize, undersize; };

inline ScreenResult screen(const Substance& in, double efficiency) {
    ScreenResult out;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double to_over = (s == GRAVEL) ? efficiency : (1.0 - efficiency);
            out.oversize.freegrain[p][s]  = in.freegrain[p][s] * to_over;
            out.undersize.freegrain[p][s] = in.freegrain[p][s] * (1.0 - to_over);
            out.oversize.composite[p][s]  = in.composite[p][s] * to_over;
            out.undersize.composite[p][s] = in.composite[p][s] * (1.0 - to_over);
        }
    return out;
}

// Crushing does two things with one blow: it breaks composites, freeing what was
// locked inside them, and it drives mass into finer bins where a pan cannot hold
// it. Both halves are one function because they are one act.
inline Substance crush(const Substance& in, double intensity) {
    Substance out = in;
    constexpr double f = COMPOSITE_TARGET_FRACTION;

    // Ascending, so mass moved into a finer bin is not moved again this blow.
    for (int s = SAND; s <= GRAVEL; ++s) {
        const int finer = s - 1;
        for (int p = 0; p < N_PHASE; ++p) {
            // Breakage liberates. The composite's two halves become free grains.
            if (p != GANGUE) {
                const double broken = out.composite[p][s] * intensity * 0.6;
                out.composite[p][s] -= broken;
                out.freegrain[p][finer]      += f * broken;
                out.freegrain[GANGUE][finer] += (1.0 - f) * broken;
            }
            // What did not break still got smaller.
            const double df = out.freegrain[p][s] * intensity * 0.5;
            out.freegrain[p][s] -= df;
            out.freegrain[p][finer] += df;

            const double dc = out.composite[p][s] * intensity * 0.5;
            out.composite[p][s] -= dc;
            out.composite[p][finer] += dc;
        }
    }
    return out;
}

// Grind, screen, return the fines to the mill. This is a closed circuit, it is
// what every mill on earth does, and the interesting failure lives inside it.
inline Substance closed_circuit(const Substance& ore, double intensity,
                                double screen_efficiency, int cycles) {
    Substance feed = ore, product;
    for (int i = 0; i < cycles; ++i) {
        const ScreenResult sr = screen(crush(feed, intensity), screen_efficiency);
        product.add(sr.undersize);
        feed = sr.oversize;
    }
    product.add(feed);
    return product;
}

inline double recovery(const Substance& feed, const Substance& conc, int p) {
    const double f = feed.phase_mass(p);
    return f > 0.0 ? conc.phase_mass(p) / f : 0.0;
}

} // namespace wrought
