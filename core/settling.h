#pragma once

#include <cmath>
#include "substance.h"

// Terminal settling velocity in water.
//
// This file exists because the first version of `separate.h` cut on *density*
// and then bolted a per-size-bin `size_efficiency` multiplier onto the result to
// express "fines wash over the lip." That multiplier was three authored numbers
// per tool standing in for physics, and it was the most likely place the model
// was simply wrong (issue #5).
//
// A pan does not cut on density. It cuts on how fast a particle falls through
// water, which is a function of density AND size together. That is why fine gold
// escapes a sluice and coarse quartz does not. Once the partition variable is
// settling velocity, `size_efficiency` is not corrected — it is deleted, and the
// behaviour it was faking falls out of a force balance.
//
// Everything here is a lookup or a force balance. Nothing is tuned.

namespace wrought {

// Fresh water at 288 K — the default `Substance::temperature`. Both of these are
// strong functions of temperature and neither is currently coupled to it.
inline constexpr double WATER_DENSITY   = 999.1;    // kg/m^3   UNVERIFIED
inline constexpr double WATER_VISCOSITY = 1.138e-3; // Pa*s     UNVERIFIED
inline constexpr double GRAVITY         = 9.80665;  // m/s^2    defined, not measured

// Wentworth (1922) grade-scale boundaries, in metres. UNVERIFIED.
// GRAVEL spans granule through cobble.
//
// The Wentworth clay class has no lower bound — it is "everything finer than
// 3.9 um". A bin needs one, because `bin_diameter` takes a geometric mean, so
// **0.2 um is AUTHORED** and it is the single most load-bearing invented number
// in the file: it sets `bin_diameter(CLAY)` to 0.88 um, and settling velocity
// goes as d^2, so halving it would quadruple every levigation time in the game.
// It is roughly the coarse end of the kaolinite platelet range. It is a stand-in
// for a size distribution we do not carry, and it should be a tracked issue and
// not a fact.
//
// What is *not* authored is the consequence: quartz at 0.88 um falls 0.10 m in
// about 45 hours, and at 15.6 um in about nine minutes. That three-hundred-fold
// gap is the whole of levigation, and nobody chose it.
//
// Not modelled: flocculation. A real clay suspension's settling behaviour is
// governed as much by surface chemistry as by Stokes — potters deflocculate to
// keep clay up and flocculate to drop it, and a floc settles as one big particle.
// We model bare grains. This makes our levigation harder than the real thing,
// which is the safe direction to be wrong in, but it is a direction.
inline constexpr double SIZE_BOUNDS[N_SIZE][2] = {
    {0.2e-6,  3.9e-6},
    {3.9e-6,  62.5e-6},
    {62.5e-6, 2.0e-3},
    {2.0e-3,  64.0e-3},
};

// One diameter stands for a whole bin. The geometric mean, because the Wentworth
// scale is logarithmic and the midpoint of a log scale is the geometric mean.
// AUTHORED in the sense that a bin has a distribution and we are taking a point
// from it; not authored in the sense that the point was chosen to make anything
// come out right.
inline double bin_diameter(int s) {
    return std::sqrt(SIZE_BOUNDS[s][0] * SIZE_BOUNDS[s][1]);
}

// Drag coefficient of a sphere.
//   Re <= 1000 : Schiller & Naumann (1933), which collapses to Stokes' 24/Re as
//                Re -> 0 and is within a few percent through the intermediate
//                regime.
//   Re >  1000 : Newton's regime, Cd ~ 0.44 and flat.
// UNVERIFIED. Real grains are not spheres; a shape factor belongs here and is
// not present.
inline double drag_coefficient(double re) {
    if (re < 1e-12) return 1e12;  // 24/Re diverges at rest; keep the solve finite
    if (re <= 1000.0) return (24.0 / re) * (1.0 + 0.15 * std::pow(re, 0.687));
    return 0.44;
}

// Solve the force balance on a falling sphere at terminal velocity,
//
//     (pi/6) d^3 (rho_p - rho_f) g  =  Cd * (pi/8) * rho_f * d^2 * v^2
//
// for v. Cd depends on Re depends on v, so this is a fixed point rather than a
// formula. Seeded with the Stokes solution and damped, because the map is not a
// contraction at high Re (the Stokes seed overshoots a gravel pebble by ~100x).
inline double settling_velocity(double particle_density_gcc, double diameter_m) {
    const double rho_p = particle_density_gcc * 1000.0;
    const double drho  = rho_p - WATER_DENSITY;
    if (drho <= 0.0) return 0.0;  // it floats. Nothing in the phase table does.

    double v = GRAVITY * diameter_m * diameter_m * drho / (18.0 * WATER_VISCOSITY);
    for (int i = 0; i < 200; ++i) {
        const double re = WATER_DENSITY * v * diameter_m / WATER_VISCOSITY;
        const double vn = std::sqrt(4.0 * GRAVITY * diameter_m * drho
                                    / (3.0 * drag_coefficient(re) * WATER_DENSITY));
        const double next = 0.5 * (v + vn);
        if (std::fabs(next - v) < 1e-14) return next;
        v = next;
    }
    return v;
}

// The closed form, for the regime where it is valid. Kept so a test can assert
// that the general solve agrees with it on fines, which is the only claim about
// `drag_coefficient` that can be checked without a wind tunnel.
inline double stokes_velocity(double particle_density_gcc, double diameter_m) {
    const double drho = particle_density_gcc * 1000.0 - WATER_DENSITY;
    return drho <= 0.0 ? 0.0
                       : GRAVITY * diameter_m * diameter_m * drho / (18.0 * WATER_VISCOSITY);
}

inline double reynolds(double v, double diameter_m) {
    return WATER_DENSITY * v * diameter_m / WATER_VISCOSITY;
}

// Every particle in the model belongs to one of 2 * N_PHASE * N_SIZE classes,
// and a class has one velocity. Solving the fixed point once per class rather
// than once per separation keeps a 256-pass sweep cheap.
namespace detail {
struct VelocityTable {
    double freegrain[N_PHASE][N_SIZE];
    double composite[N_PHASE][N_SIZE];
    VelocityTable() {
        for (int p = 0; p < N_PHASE; ++p) {
            const double rho_c = composite_density(p);
            for (int s = 0; s < N_SIZE; ++s) {
                const double d = bin_diameter(s);
                freegrain[p][s] = settling_velocity(PHASES[p].density, d);
                composite[p][s] = settling_velocity(rho_c, d);
            }
        }
    }
};
inline const VelocityTable& table() { static const VelocityTable t; return t; }
} // namespace detail

inline double free_velocity(int p, int s)      { return detail::table().freegrain[p][s]; }
inline double composite_velocity(int p, int s) { return detail::table().composite[p][s]; }

} // namespace wrought
