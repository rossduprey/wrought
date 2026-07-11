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
    // A separator has TWO misplacements, not one. `sharpness` blurs the cut; this
    // is the other one. On a rough floor a grain finer than the floor grit wedges
    // between its elements, feels no flow, and does not move at any cut -- so it
    // stays in the pan (reports to the concentrate) whatever its velocity, unsorted
    // feed diluting the product. `shelter[s]` is the fraction of size bin `s` so
    // wedged. Zero for any smooth tool (hands, a built sluice, an ideal pan); a
    // fired pan fills it from its floor grit (fire.h `fire_pan`). This is the same
    // two-misplacement lesson `screen()` learned about size on 2026-07-09 and
    // `fire.h` learned about floors on 2026-07-10 -- now APPLIED, not just counted
    // (#19). A smooth default leaves every existing caller unchanged.
    double shelter[N_SIZE] = {};
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

    const double T = in.temperature;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            // The second misplacement. A `shelter[s]` share of this bin is wedged
            // in the floor, feels no flow, and stays in the pan whatever its
            // velocity; only the exposed remainder is actually sorted by the cut.
            const double phi = sp.shelter[s];

            const double mf = in.freegrain[p][s];
            const double cf = mf * phi + mf * (1.0 - phi) * partition(free_velocity(p, s, T), sp);
            out.concentrate.freegrain[p][s] = cf;
            out.tailings.freegrain[p][s]    = mf - cf;

            // A composite reports as one particle, at one velocity, and its
            // gangue is not consulted. This is where the grade ceiling comes
            // from. It shelters by size just as a free grain does.
            const double mc = in.composite[p][s];
            const double cc = mc * phi + mc * (1.0 - phi) * partition(composite_velocity(p, s, T), sp);
            out.concentrate.composite[p][s] = cc;
            out.tailings.composite[p][s]    = mc - cc;
        }
    return out;
}

// The bed.
//
// separate() above assumes every particle in the pan sees the same water. It does
// not. Grains settle at their terminal velocity and the swirl stirs them back up,
// and the equilibrium between those two is the same one that sets the density of
// the atmosphere with height: an exponential profile,
//
//     c(z) proportional to exp(-v*z/D)
//
// with D the shear-induced diffusivity. Sediment transport calls this the Rouse
// profile and has been measuring it since 1937. Only the top of the pan is in the
// moving water; everything under it is a bed, and a bed cannot be washed away.
//
// So the fraction of a grain population sitting in the exposed skin is a Boltzmann
// factor in settling velocity, exp(-v/v_mix), where v_mix = D/h absorbs the skin
// depth. **The pan needs no vertical dimension.** The vertical dimension is
// derived from velocity, exactly as liberation is derived from the free/composite
// split and never stored.
//
// v_mix is not a free parameter either. D ~ u* h, so v_mix ~ u*, the shear
// velocity -- which is the cut. Swirl hard and everything mixes and everything is
// exposed; swirl gently and the magnetite hides underneath the quartz. It is the
// number already on the screen. Nothing is authored here but the geometry of the
// pan itself.
//
// This is why a real panner does not lose his gold. While 1900 g of quartz lies on
// top of the magnetite, the magnetite is buried and cannot leave the pan at any
// cut. It becomes exposed only as the quartz departs. The saturation is in the
// normalisation, and nobody put it there.
//
// *(Added 2026-07-09. Found by watching a live recovery column drain to 3% on a
// pan that had been washed for a minute -- which is false, and which the test
// suite could not see, because a test calls separate() a fixed number of times.)*

// A pan is about 30 cm across; the water strips a skin a few grain diameters
// deep; wet sand packs at roughly 1600 kg/m^3. AUTHORED, and each factor is a
// thing a person could go and measure with a ruler and a bucket.
inline constexpr double PAN_RADIUS   = 0.15;   // m
inline constexpr double SKIN_GRAINS  = 3.0;    // skin depth, in sand-grain diameters
inline constexpr double BULK_DENSITY = 1600.0; // kg/m^3, packed wet sand. UNVERIFIED.

// How deep the moving water reaches. This used to live in `fire.h`, because the
// grit->sharpness bridge was written in units of it. That bridge no longer knows
// what a skin is -- roughness broadens a partition by displacing the grain the
// separator is deciding about, and the depth of the moving water has nothing to
// say about it (#10, 2026-07-10). The skin is a property of the flow, and this is
// where the flow lives.
inline double skin_depth() { return SKIN_GRAINS * bin_diameter(SAND); }

inline double skin_mass() {
    return M_PI * PAN_RADIUS * PAN_RADIUS * skin_depth() * BULK_DENSITY;
}

// The part of the pan the water can actually reach, this instant, at this cut.
// Still water reaches nothing: as v_mix -> 0 every weight underflows and the bed
// is the whole pan. That is the same physical statement as the cut clamp in
// pan.cpp, arrived at from the other side.
inline Substance exposed(const Substance& pan, double v_mix, double skin) {
    Substance top;
    if (v_mix <= 0.0 || skin <= 0.0) return top;

    const double T = pan.temperature;
    double w[N_PHASE][N_SIZE][2], total = 0.0;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            w[p][s][0] = std::exp(-free_velocity(p, s, T) / v_mix);
            w[p][s][1] = std::exp(-composite_velocity(p, s, T) / v_mix);
            total += pan.freegrain[p][s] * w[p][s][0] + pan.composite[p][s] * w[p][s][1];
        }
    if (total <= 0.0) return top;

    // Fill the skin. A cell cannot contribute more than it has, which is what
    // caps the whole pan being in the skin once the pan is smaller than one.
    const double k = skin / total;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            top.freegrain[p][s] = pan.freegrain[p][s] * std::fmin(1.0, w[p][s][0] * k);
            top.composite[p][s] = pan.composite[p][s] * std::fmin(1.0, w[p][s][1] * k);
        }
    return top;
}

// A screen is a separation by size and nothing else. It is the verb that makes
// crushing worth doing, and Era 0 in DESIGN.md did not have it.
//
// `cut_bin` is the finest bin that reports oversize. Two screens matter, and both
// are Era 0: scalping (cut_bin = GRAVEL, take the pebbles out) and desliming
// (cut_bin = SAND, wash the mud — clay and silt both — off). A gravity separator needs both, because it
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
    // Everything above the finest bin; the finest has nowhere to go. This used
    // to read `s = SAND`, which meant the same thing only while there were three
    // bins and SAND was the second of them.
    for (int s = 1; s < N_SIZE; ++s) {
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
