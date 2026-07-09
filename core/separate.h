#pragma once

#include <cmath>
#include "settling.h"

// Gravity separation as a partition function.
//
// DESIGN.md describes progression as a grade/recovery curve that a better tool
// "moves outward". A curve you author is a curve you balance, which the source
// rule forbids. So nothing here authors a curve: a separator is the probability
// that a particle reports to the concentrate, as a function of that particle's
// terminal settling velocity. The grade/recovery curve is an *output* of two
// numbers.
//
//   cut_velocity — the operator's call. Wash harder, raise the cut.
//   sharpness    — the tool. A duller tool blurs the cut, and no amount of skill
//                  repairs it. This is what "the tool moves the curve" means.
//
// The partition is logistic in ln(v), not in v, because settling velocity spans
// four orders of magnitude across the size bins and classifier partition curves
// are log-normal in practice. That choice buys a reportable, falsifiable
// definition of sharpness: with x = ln(v/v50)/sigma, the quartiles sit at
// v75/v25 = 9^sigma. Mineral processing calls that ratio the imperfection of the
// separation, and it is a thing that gets measured, unlike a sigma we invented.
//
//   hands  9^1.20 = 13.5      pan  9^0.55 = 3.1      sluice  9^0.22 = 1.6
//
// Those exponents are AUTHORED (issue #5). The ordering is what the tests
// assert; the magnitudes are a standing invitation to be corrected by anyone
// holding a real Tromp curve for a sluice box.
//
// Note what is NOT here any more: a per-size-bin efficiency. The first version
// of this file cut on density and multiplied the result by size_efficiency[] to
// make fines wash away. Cutting on velocity makes fines wash away because a fine
// grain falls slowly, which is the actual reason. Three authored numbers per
// tool were deleted rather than cited.

namespace wrought {

struct SeparatorParams {
    double cut_velocity;  // m/s, v50
    double sharpness;     // sigma in ln-velocity; 0 is a perfect cut
    const char* name;
};

// Cupped hands, then a low-fired pinch pot, then a built sluice. One function.
inline constexpr SeparatorParams HANDS  = {0.060, 1.20, "hands"};
inline constexpr SeparatorParams PAN    = {0.060, 0.55, "pan"};
inline constexpr SeparatorParams SLUICE = {0.060, 0.22, "sluice"};

struct SeparationResult {
    Substance concentrate;
    Substance tailings;
};

// Fast particles stay in the pan. Slow ones go over the lip.
inline double partition(double v, const SeparatorParams& sp) {
    if (v <= 0.0) return 0.0;
    const double x = std::log(v / sp.cut_velocity) / sp.sharpness;
    return 1.0 / (1.0 + std::exp(-x));
}

// The quartile ratio v75/v25 of this tool. A separator's imperfection, in the
// units a mineral processing paper would report it in.
inline double imperfection(const SeparatorParams& sp) {
    return std::pow(9.0, sp.sharpness);
}

inline SeparationResult separate(const Substance& in, const SeparatorParams& sp) {
    SeparationResult out;
    out.concentrate.temperature = out.tailings.temperature = in.temperature;

    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double cf = in.freegrain[p][s] * partition(free_velocity(p, s), sp);
            out.concentrate.freegrain[p][s] = cf;
            out.tailings.freegrain[p][s]    = in.freegrain[p][s] - cf;

            // A composite reports as one particle, at one velocity, and its
            // gangue is not consulted. This is where the grade ceiling comes
            // from.
            const double cc = in.composite[p][s] * partition(composite_velocity(p, s), sp);
            out.concentrate.composite[p][s] = cc;
            out.tailings.composite[p][s]    = in.composite[p][s] - cc;
        }
    return out;
}

// A screen is a separation by size and nothing else. It is the verb that makes
// crushing worth doing, and Era 0 in DESIGN.md did not have it.
//
// `cut_bin` is the finest bin that reports oversize. Two screens matter, and both
// are Era 0: scalping (cut_bin = GRAVEL, take the pebbles out) and desliming
// (cut_bin = SAND, wash the mud off). A gravity separator needs both, because it
// obeys the grade/recovery law only within one size class. `separate()` will
// happily concentrate a pebble.
//
// A separator has **two** misplacements, not one.
//
// *(Corrected 2026-07-09, found by playing `make pan` rather than by a test.)*
// This used to take a single `efficiency` and use it in both directions: coarse
// reported oversize with probability e, and fine was misplaced oversize with
// probability 1-e. That is a symmetric special case with no reason to be true,
// and it makes hand-picking pebbles out of a pan cost 15% of your sand per pick.
// It does not. A real screen's Tromp curve reports the two independently, and so
// do we now. The old one-argument call survives as the symmetric case.
struct ScreenParams {
    double coarse_to_oversize;  // recovery of what belongs on top
    double fine_to_oversize;    // misplacement of what belongs below
    int    cut_bin;             // finest bin that belongs on top
};
struct ScreenResult { Substance oversize, undersize; };

inline ScreenResult screen(const Substance& in, const ScreenParams& sp) {
    ScreenResult out;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double to_over = (s >= sp.cut_bin) ? sp.coarse_to_oversize : sp.fine_to_oversize;
            out.oversize.freegrain[p][s]  = in.freegrain[p][s] * to_over;
            out.undersize.freegrain[p][s] = in.freegrain[p][s] * (1.0 - to_over);
            out.oversize.composite[p][s]  = in.composite[p][s] * to_over;
            out.undersize.composite[p][s] = in.composite[p][s] * (1.0 - to_over);
        }
    return out;
}

inline ScreenResult screen(const Substance& in, double efficiency, int cut_bin = GRAVEL) {
    return screen(in, ScreenParams{efficiency, 1.0 - efficiency, cut_bin});
}

// Cobbing: picking stones out of a pan with your fingers. DESIGN.md Era 0 calls
// it "high grade, near-zero throughput, coarse liberated material only." Those
// are exactly the two numbers above and they are nothing like each other. You
// miss a lot of pebbles; you drop almost no sand. AUTHORED, UNVERIFIED — and
// note that a single-efficiency screen *cannot represent this verb at all*,
// which is how the defect was found.
inline constexpr ScreenParams HAND_COB = {0.60, 0.008, GRAVEL};

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
