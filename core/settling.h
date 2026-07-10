#pragma once

#include <cmath>
#include <map>
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
// It solved that force balance on a **sphere**, and clay is not a sphere. That
// was issue #13, and closing it moved a finding rather than refining one. See
// "What a size bin's diameter means" below.
//
// Everything here is a lookup or a force balance. Nothing is tuned.

namespace wrought {

inline constexpr double GRAVITY = 9.80665;  // m/s^2   defined, not measured

// The default `Substance::temperature`. A river in the model is 15 C until
// something says otherwise.
inline constexpr double REFERENCE_TEMPERATURE = 288.0;  // K

// ---------------------------------------------------------------------------
// Water. Both properties are strong functions of temperature, and until 2026-07-10
// both were hardcoded at 288 K while `Substance::temperature` sat unread in the
// struct. A hot spring and a snowmelt creek panned identically. They no longer do:
// water at 1 C is 1.75x as viscous as water at 40 C, so a clay particle takes
// 2.6x as long to fall the same distance in a snowmelt creek. That is now in the
// model, and it costs nothing, because the number was always in the struct.
// ---------------------------------------------------------------------------

// Kell, G.S. (1975), "Density, thermal expansivity, and compressibility of liquid
// water from 0 deg to 150 deg C", J. Chem. Eng. Data 20(1), 97-105. Stated
// accuracy +/- 0.02 kg/m^3 over 0-100 C at 1 atm; t is IPTS-68 Celsius, which
// differs from ITS-90 by less than the fit's own error over this range.
//
// The test suite does not take this on trust. It checks the fit against four
// published densities AND against a number the fit was never given: that liquid
// water is densest at 3.98 C. The polynomial puts the maximum at 3.983 C. Nobody
// fitted that; it falls out.
inline double water_density(double kelvin) {
    const double t = kelvin - 273.15;
    const double num = 999.83952
                     + 16.945176      * t
                     -  7.9870401e-3  * t * t
                     - 46.170461e-6   * t * t * t
                     + 105.56302e-9   * t * t * t * t
                     - 280.54253e-12  * t * t * t * t * t;
    return num / (1.0 + 16.879850e-3 * t);
}

// Vogel-Fulcher-Tammann fit, mu = A * 10^(B/(T - C)). Quoted for water with
// A = 2.414e-5 Pa*s, B = 247.8 K, C = 140 K, and stated to hold within 2.5% over
// 0-370 C; see Al-Shemmeri, "Engineering Fluid Mechanics" (2012), and the
// underlying Vogel (1921) / Fulcher (1925) form.
//
// Measured against published values it is better than that near river
// temperatures and worse at the freezing point: -0.13% at 15 C, +0.01% at 20 C,
// -2.14% at 0 C. The 0 C error is the largest number in this file that we know
// to be wrong and have chosen to keep, because the alternative is the full IAPWS
// formulation and no process in the game runs on ice water.
//
// Note the sign. It is 10^(B/(T - C)), not (T + C); the latter yields 9.0e-5 Pa*s
// at 20 C against a true 1.002e-3, and is a common transcription error.
inline double water_viscosity(double kelvin) {
    return 2.414e-5 * std::pow(10.0, 247.8 / (kelvin - 140.0));
}

// Wentworth (1922) grade-scale boundaries, in metres. UNVERIFIED.
// GRAVEL spans granule through cobble.
//
// The Wentworth clay class has no lower bound — it is "everything finer than
// 3.9 um". A bin needs one, because `bin_diameter` takes a geometric mean, so
// **0.2 um is AUTHORED** and it is the single most load-bearing invented number
// in the file: it sets `bin_diameter(CLAY)` to 0.88 um, and settling velocity
// goes as d^2, so halving it would quadruple every levigation time in the game.
// It is a stand-in for a size distribution we do not carry, and it should be a
// tracked issue and not a fact.
//
// It is also, by luck rather than by design, almost exactly the fine end of the
// measured kaolinite platelet face-diameter range (0.1-4.0 um) against this
// bin's 0.2-3.9 um. That coincidence is an argument, and it is used below.
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

// ---------------------------------------------------------------------------
// What a size bin's diameter means
//
// This is the bridge, and it is worth more than any number in the file.
//
// `bin_diameter(CLAY)` is 0.88 um. Of *what*? A sphere has one diameter and the
// question does not arise. A kaolinite platelet ten times wider than it is thick
// has at least three answers, and they differ by a factor of seven in settling
// velocity:
//
//   (A) the diameter of the sphere of equal VOLUME.       ratio -> 1.39x
//   (B) the diameter of the plate's FACE.                 ratio -> 6.98x
//   (C) the diameter of the sphere of equal SETTLING      ratio -> 1.00x
//       VELOCITY, which is what a hydrometer reports.
//
// where "ratio" is v_quartz / v_kaolinite within the CLAY bin — the number that
// decides whether levigation can separate clay from clay-sized quartz at all,
// and therefore whether Era 1 needs deflocculation (issue #15). Under (A) it
// cannot and it does. Under (B) it can and it does not.
//
// We take (B). Three reasons, in ascending order of force:
//
//   (C) is a tautology wearing rigour. It defines a size class BY settling
//   velocity, which makes "a velocity separator cannot separate within a class"
//   true by construction rather than by physics. The bin definition would be
//   doing the work the force balance is here to do. This project derives
//   velocity; it does not define size by it.
//
//   Given geometric bins, a bin diameter is a LINEAR dimension — a sieve
//   aperture, an intermediate axis, a projected outline. For an oblate spheroid
//   the intermediate axis is the face diameter. And the measured kaolinite face
//   diameters, 0.1-4.0 um, land on this bin's 0.2-3.9 um. When a mineralogist
//   says "clay-sized", the number reported is the long dimension.
//
//   Reality settles it. Potters levigate dirt and get clay pure enough to throw.
//   A model in which levigation can never lift clay above the deposit's own
//   84.3% is contradicted by every potter who has ever lived. The sphere was why.
//
// So: `bin_diameter(s)` is a grain's face diameter, and the volume-equivalent
// diameter is DERIVED from it and the aspect ratio, exactly, by d_v = d * p^(1/3).
// For p = 1 the two coincide and every sphere in the phase table keeps the
// velocity it had before this file was touched.
// ---------------------------------------------------------------------------

// Exact for a spheroid: equal volume means (pi/6) d_v^3 = (pi/6) d^3 * p.
inline double volume_equivalent_diameter(double face_diameter, double p) {
    return face_diameter * std::cbrt(p);
}

// Sphericity of an oblate spheroid of axial ratio p: surface area of the
// equal-volume sphere over the actual surface area. Pure geometry, no citation,
// no fit. Reduces to 1 at p = 1 and falls to 0.093 at p = 0.01.
inline double oblate_sphericity(double p) {
    if (p >= 1.0) return 1.0;
    const double e = std::sqrt(1.0 - p * p);
    const double area = 2.0 * M_PI * (1.0 + ((1.0 - e * e) / e) * std::atanh(e));
    return 4.0 * M_PI * std::cbrt(p) * std::cbrt(p) / area;
}

// The Stokes shape factor: v_shaped / v_equal-volume-sphere in creeping flow.
//
// This is the reciprocal of the Perrin translational friction factor for an
// oblate spheroid, orientation-averaged. Perrin, F. (1936), J. Phys. Radium 7,
// 1-11; the same result is Oberbeck's (1876), and it is tabulated in Happel &
// Brenner, "Low Reynolds Number Hydrodynamics" (1965), ch. 5.
//
//   xi = sqrt(1 - p^2) / p ,   S = 2 arctan(xi) / xi ,   f_P = 2 p^(2/3) / S
//
// It is EXACT. There is no fit and no free parameter, which makes it the second
// thing in this project after levigation's ramp to have none.
//
// Derived, and used as the test: as p -> 0 the total friction must approach the
// exact thin-disk result 12 * mu * a, which is a different calculation from a
// different century. It does, to five figures. Nobody arranged that.
//
// The magnitude is the surprise. A 10:1 plate settles at 0.686x the equal-volume
// sphere — a 1.46x slowdown, not the 3x issue #13 hoped for. Shape alone is a
// weak effect. What is strong is the VOLUME the face diameter implies: a plate of
// face diameter d has the volume of a sphere of diameter 0.464 d, and velocity
// goes as d^2. Nearly all of the 6.98x is that, and only 1.46x of it is drag.
inline double stokes_shape_factor(double p) {
    if (p >= 1.0) return 1.0;
    const double xi = std::sqrt(1.0 - p * p) / p;
    const double S  = 2.0 * std::atan(xi) / xi;
    return S / (2.0 * std::cbrt(p) * std::cbrt(p));
}

// Ganser's Newton shape factor, K2 = 10^(1.8148 * (-log10 phi)^0.5743).
// Ganser, G.H. (1993), "A rational approach to drag prediction of spherical and
// nonspherical particles", Powder Technology 77, 143-152.
//
// This one IS a fit, and it is the only fitted shape number in the file. It
// governs the Newton regime, which in this game means gravel. Nothing platy is
// ever in the gravel bin, so it is load-bearing for nothing and is here for
// completeness of the drag law rather than because a result depends on it.
inline double newton_shape_factor(double sphericity) {
    if (sphericity >= 1.0) return 1.0;
    return std::pow(10.0, 1.8148 * std::pow(-std::log10(sphericity), 0.5743));
}

// Drag coefficient of a SPHERE.
//   Re <= 1000 : Schiller & Naumann (1933), Z. Ver. Dtsch. Ing. 77, 318-320,
//                which collapses to Stokes' 24/Re as Re -> 0 and is within a few
//                percent of the standard drag curve through the intermediate
//                regime.
//   Re >  1000 : Newton's regime, Cd ~ 0.44 and flat.
// UNVERIFIED: the coefficients 0.15 and 0.687 and the crossover at Re 1000.
inline double sphere_drag_coefficient(double re) {
    if (re < 1e-12) return 1e12;  // 24/Re diverges at rest; keep the solve finite
    if (re <= 1000.0) return (24.0 / re) * (1.0 + 0.15 * std::pow(re, 0.687));
    return 0.44;
}

// Drag coefficient of a spheroid, by Ganser's rescaling of the sphere curve:
//
//     Cd(Re) = K2 * Cd_sphere(Re * K1 * K2)
//
// That rescaling — not the particular curve Ganser fitted to it — is the paper's
// physical claim: one generalised drag curve, stretched on both axes by a
// Stokes-regime shape factor and a Newton-regime one. It is exact in both limits
// by construction: Cd -> 24/(Re K1) as Re -> 0, and Cd -> 0.44 K2 as Re -> inf.
//
// We do NOT use Ganser's own sphere fit, and the reason is measured rather than
// aesthetic. His curve, evaluated at K1 = K2 = 1, gives Cd = 2.43 at Re = 17 —
// the SAND bin — where Schiller & Naumann give 2.88 and the standard drag curve
// gives about 2.85. Adopting his equation wholesale would have moved the sluice's
// enrichment from 33.7x to 39.6x by making the SPHERE case worse in order to fix
// the platelet case. Instead K1 and K2 rescale the sphere law that was already
// here, and every p = 1 phase in the table keeps the velocity it had.
//
// Ganser's K1 fit for isometric particles, (1/3 + (2/3) phi^-0.5)^-1, is not used
// either: for the one phase where K1 differs from unity we have the exact value
// from Perrin, and an exact number does not need a correlation. Dioguardi et al.
// (2018), JGR Solid Earth 123, 144-156, measure Ganser underestimating Cd below
// Re 0.1 for irregular grains, which is precisely the regime clay lives in and
// precisely why the exact solution is worth having there.
inline double drag_coefficient(double re, double K1, double K2) {
    return K2 * sphere_drag_coefficient(re * K1 * K2);
}

// Solve the force balance on a falling spheroid at terminal velocity,
//
//     (pi/6) d_v^3 (rho_p - rho_f) g  =  Cd * (pi/8) * rho_f * d_v^2 * v^2
//
// for v, where d_v is the volume-equivalent diameter. Cd depends on Re depends on
// v, so this is a fixed point rather than a formula. Seeded with the shape-
// corrected Stokes solution and damped, because the map is not a contraction at
// high Re (the Stokes seed overshoots a gravel pebble by ~100x).
//
// `face_diameter` is the bin diameter. `p` is the aspect ratio. See the bridge
// comment above for why those are not the same thing.
inline double settling_velocity(double particle_density_gcc, double face_diameter,
                                double p = 1.0, double kelvin = REFERENCE_TEMPERATURE) {
    const double rho_f = water_density(kelvin);
    const double mu    = water_viscosity(kelvin);
    const double rho_p = particle_density_gcc * 1000.0;
    const double drho  = rho_p - rho_f;
    if (drho <= 0.0) return 0.0;  // it floats. Nothing in the phase table does.

    const double d  = volume_equivalent_diameter(face_diameter, p);
    const double K1 = stokes_shape_factor(p);
    const double K2 = newton_shape_factor(oblate_sphericity(p));

    double v = K1 * GRAVITY * d * d * drho / (18.0 * mu);
    for (int i = 0; i < 200; ++i) {
        const double re = rho_f * v * d / mu;
        const double vn = std::sqrt(4.0 * GRAVITY * d * drho
                                    / (3.0 * drag_coefficient(re, K1, K2) * rho_f));
        const double next = 0.5 * (v + vn);
        if (std::fabs(next - v) < 1e-14) return next;
        v = next;
    }
    return v;
}

// The closed form, for the regime where it is valid. Kept so a test can assert
// that the general solve agrees with it on fines, which is the only claim about
// `sphere_drag_coefficient` that can be checked without a wind tunnel.
inline double stokes_velocity(double particle_density_gcc, double face_diameter,
                              double p = 1.0, double kelvin = REFERENCE_TEMPERATURE) {
    const double rho_f = water_density(kelvin);
    const double mu    = water_viscosity(kelvin);
    const double drho  = particle_density_gcc * 1000.0 - rho_f;
    if (drho <= 0.0) return 0.0;
    const double d = volume_equivalent_diameter(face_diameter, p);
    return stokes_shape_factor(p) * GRAVITY * d * d * drho / (18.0 * mu);
}

// Reynolds number on the volume-equivalent diameter, which is the length Ganser's
// rescaling is defined against.
inline double reynolds(double v, double face_diameter, double p = 1.0,
                       double kelvin = REFERENCE_TEMPERATURE) {
    const double d = volume_equivalent_diameter(face_diameter, p);
    return water_density(kelvin) * v * d / water_viscosity(kelvin);
}

// Every particle in the model belongs to one of 2 * N_PHASE * N_SIZE classes, and
// a class has one velocity at one temperature. Solving the fixed point once per
// class rather than once per separation keeps a 256-pass sweep cheap; a substance
// carries one temperature, so a sweep touches one table.
//
// Composite grains take p = 1 and it is a modelling claim, not an oversight: a
// composite is a lump of two minerals grown together, and a lump is blocky no
// matter what its constituents cleave like. A kaolinite platelet is a platelet
// because it is free. Liberation gives a grain its shape back.
// The aspect ratio of a free grain of phase p in size bin s. A crystal habit has
// a size: kaolinite is a platelet where kaolinite crystals are, which is the CLAY
// bin, and a lump of stacked platelets everywhere above it.
inline double grain_aspect_ratio(int p, int s) {
    return bin_diameter(s) <= PHASES[p].platy_below ? PHASES[p].aspect_ratio : 1.0;
}

namespace detail {
struct VelocityTable {
    double freegrain[N_PHASE][N_SIZE];
    double composite[N_PHASE][N_SIZE];
    explicit VelocityTable(double kelvin) {
        for (int p = 0; p < N_PHASE; ++p) {
            const double rho_c = composite_density(p);
            for (int s = 0; s < N_SIZE; ++s) {
                const double d = bin_diameter(s);
                freegrain[p][s] = settling_velocity(PHASES[p].density, d,
                                                    grain_aspect_ratio(p, s), kelvin);
                composite[p][s] = settling_velocity(rho_c, d, 1.0, kelvin);
            }
        }
    }
};
inline const VelocityTable& table(double kelvin) {
    static std::map<long long, VelocityTable> cache;
    const long long key = std::llround(kelvin * 1e6);
    auto it = cache.find(key);
    if (it == cache.end()) it = cache.emplace(key, VelocityTable(kelvin)).first;
    return it->second;
}
} // namespace detail

inline double free_velocity(int p, int s, double kelvin = REFERENCE_TEMPERATURE) {
    return detail::table(kelvin).freegrain[p][s];
}
inline double composite_velocity(int p, int s, double kelvin = REFERENCE_TEMPERATURE) {
    return detail::table(kelvin).composite[p][s];
}

} // namespace wrought
