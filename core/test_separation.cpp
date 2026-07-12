// Phase A, step 1: pan a bucket of river sand.
//
// DESIGN.md proposes conservation of mass as the unit test. It is necessary and
// far too weak — a separator that does nothing conserves mass perfectly. Every
// test below asserts a *relationship*, so none of them can be satisfied by
// tuning the fixture, and correcting a density must not change any outcome.
//
// This suite has been rewritten once. The first version separated on particle
// density with a hand-set per-size-bin efficiency multiplier. Two of its tests
// were passing for the wrong reason, and are now gone:
//
//   "even the gentlest pan enriches"  — false. A cut below every particle in the
//       feed is a no-op and returns the feed. The old test only passed because
//       size_efficiency rejected fines unconditionally, which is to say the
//       fudge factor was doing the separating.
//
//   "raising the cut trades recovery for grade, at every step" — true only
//       *within one size class*. On a feed that spans two, the curve is lumpy.
//       The old test never noticed because it never sized the feed.
//
// Build: make test

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <initializer_list>

#include "fire.h"
#include "magnetic.h"
#include "smelt.h"
#include "forge.h"
#include "geology.h"
#include "fuel.h"
#include "char.h"
#include "knap.h"
#include "haft.h"

using namespace wrought;

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "  pass" : "* FAIL", what);
    if (!ok) ++failures;
}

static const char* sz(int s) {
    return s == CLAY ? "clay" : s == SILT ? "silt" : s == SAND ? "sand" : "gravel";
}

// Author a scoop by phase mass and liberation; the struct stores particles.
// Composites drag gangue, so the gangue they carry is deducted from free quartz.
struct ScoopBuilder {
    Substance s;
    double gangue_demand[N_SIZE] {};
    double gangue_authored[N_SIZE] {};

    void put(int p, const double m[N_SIZE], const double lib[N_SIZE]) {
        constexpr double f = COMPOSITE_TARGET_FRACTION;
        if (p == GANGUE) {
            for (int k = 0; k < N_SIZE; ++k) gangue_authored[k] += m[k];
            return;
        }
        for (int k = 0; k < N_SIZE; ++k) {
            s.freegrain[p][k] = m[k] * lib[k];
            s.composite[p][k] = m[k] * (1.0 - lib[k]) / f;
            gangue_demand[k] += (1.0 - f) * s.composite[p][k];
        }
    }
    Substance build() {
        for (int k = 0; k < N_SIZE; ++k) {
            const double freeq = gangue_authored[k] - gangue_demand[k];
            if (freeq < 0.0) { std::printf("* FAIL  scoop over-committed gangue in bin %d\n", k); ++failures; }
            s.freegrain[GANGUE][k] = std::fmax(0.0, freeq);
        }
        return s;
    }
};

// The river already ran a separation pass before the character arrived: flowing
// water sorts free grains by density, and cannot sort what is still locked.
static Substance river_sand() {
    ScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {10.0*frac*0.03, 10.0*frac*0.12, 10.0*frac*0.75, 10.0*frac*0.10};
        const double l[N_SIZE] = {lf, lf, ls, lg};
        b.put(p, m, l);
    };
    row(FELDSPAR,  0.100, 0.95, 0.90, 0.75);
    row(KAOLINITE, 0.040, 1.00, 0.90, 0.60);
    row(MAGNETITE, 0.080, 0.95, 0.88, 0.55);
    row(HEMATITE,  0.010, 0.90, 0.80, 0.50);
    row(GOETHITE,  0.020, 0.90, 0.80, 0.50);
    row(ILMENITE,  0.030, 0.92, 0.85, 0.55);
    row(PYRITE,    0.005, 0.85, 0.80, 0.50);
    row(CALCITE,   0.015, 0.90, 0.85, 0.60);
    row(QUARTZ,    0.700, 1.00, 1.00, 1.00);
    return b.build();
}

// A struck outcrop. Iron is here, and almost none of it is free.
static Substance weathered_outcrop() {
    ScoopBuilder b;
    const double q[N_SIZE] = {0.04, 0.16, 1.50, 5.30}, ql[N_SIZE] = {1, 1, 1, 1};
    const double f[N_SIZE] = {0.01, 0.04, 0.30, 1.00}, fl[N_SIZE] = {0.90, 0.90, 0.60, 0.30};
    const double m[N_SIZE] = {0.004, 0.016, 0.15, 1.30}, ml[N_SIZE] = {0.85, 0.85, 0.35, 0.10};
    const double h[N_SIZE] = {0.002, 0.008, 0.05, 0.12}, hl[N_SIZE] = {0.80, 0.80, 0.30, 0.10};
    b.put(FELDSPAR, f, fl); b.put(MAGNETITE, m, ml); b.put(HEMATITE, h, hl); b.put(QUARTZ, q, ql);
    return b.build();
}

// Scalp the pebbles off the top, wash the mud off the bottom. What remains is one
// size class, which is the only feed a gravity separator obeys a law on.
static Substance sized(const Substance& s) {
    return screen(screen(s, 1.0, GRAVEL).undersize, 1.0, SAND).oversize;
}

struct Point { double cut, rec, grade; };

static std::vector<Point> frontier(const Substance& feed, const SeparatorParams& sp, int target) {
    std::vector<Point> pts;
    for (double lv = std::log(1e-4); lv <= std::log(3.0); lv += 0.02) {
        SeparatorParams q = sp; q.cut_velocity = std::exp(lv);
        const SeparationResult r = separate(feed, q);
        if (r.concentrate.total_mass() <= 1e-9) continue;
        pts.push_back({q.cut_velocity, recovery(feed, r.concentrate, target), r.concentrate.grade(target)});
    }
    return pts;
}

static bool grade_at_recovery(const std::vector<Point>& f, double rec, double& out) {
    for (size_t i = 1; i < f.size(); ++i) {
        const double a = f[i-1].rec, b = f[i].rec;
        if ((rec <= a && rec >= b) || (rec >= a && rec <= b)) {
            const double t = (std::fabs(a - b) < 1e-12) ? 0.0 : (rec - a) / (b - a);
            out = f[i-1].grade + t * (f[i].grade - f[i-1].grade);
            return true;
        }
    }
    return false;
}

static double peak_grade_over_passes(const Substance& s, const SeparatorParams& sp, int maxp, int target) {
    double best = 0.0;
    Substance cur = s;
    for (int i = 0; i < maxp; ++i) { cur = separate(cur, sp).concentrate; best = std::fmax(best, cur.grade(target)); }
    return best;
}

// Relax magnetite's liberation to a uniform value, holding phase masses fixed.
static Substance with_magnetite_liberation(double L) {
    ScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {10.0*frac*0.03, 10.0*frac*0.12, 10.0*frac*0.75, 10.0*frac*0.10};
        const double l[N_SIZE] = {lf, lf, ls, lg};
        b.put(p, m, l);
    };
    row(FELDSPAR,  0.100, 0.95, 0.90, 0.75);
    row(KAOLINITE, 0.040, 1.00, 0.90, 0.60);
    row(MAGNETITE, 0.080, L, L, L);
    row(HEMATITE,  0.010, 0.90, 0.80, 0.50);
    row(GOETHITE,  0.020, 0.90, 0.80, 0.50);
    row(ILMENITE,  0.030, 0.92, 0.85, 0.55);
    row(PYRITE,    0.005, 0.85, 0.80, 0.50);
    row(CALCITE,   0.015, 0.90, 0.85, 0.60);
    row(QUARTZ,    0.700, 1.00, 1.00, 1.00);
    return b.build();
}

// Era 0's whole dressing circuit: grind in closed circuit, scalp, deslime, pan.
static Substance dress(const Substance& ore, double intensity, double screen_eff, int cycles) {
    return sized(closed_circuit(ore, intensity, screen_eff, cycles));
}
static double dressed_recovery(const Substance& ore, double intensity, double eff, int cycles, int target) {
    return recovery(ore, separate(dress(ore, intensity, eff, cycles), PAN).concentrate, target);
}

int main() {
    const Substance raw  = river_sand();
    const Substance feed = sized(raw);

    // ---- 0. The partition variable is settling velocity, and it is solved ----
    // A pan does not cut on density. It cuts on how fast a grain falls through
    // water, which is density and size together. Everything in this block is a
    // consequence of a force balance, and none of it can be tuned.
    {
        bool stokes_fines = true;
        for (int p = 0; p < N_PHASE; ++p)
            for (int b : {CLAY, SILT}) {
                const double d = bin_diameter(b);
                const double g = settling_velocity(PHASES[p].density, d), s = stokes_velocity(PHASES[p].density, d);
                if (std::fabs(s / g - 1.0) > 0.01) stokes_fines = false;
            }
        check(stokes_fines, "on clay and silt, the drag solve agrees with Stokes' law to within 1%");

        const double d_sand = bin_diameter(SAND), d_grav = bin_diameter(GRAVEL);
        const double r_sand = stokes_velocity(PHASES[QUARTZ].density, d_sand) / settling_velocity(PHASES[QUARTZ].density, d_sand);
        const double r_grav = stokes_velocity(PHASES[QUARTZ].density, d_grav) / settling_velocity(PHASES[QUARTZ].density, d_grav);
        check(r_sand > 1.5 && r_grav > 10.0,
              "on sand and gravel Stokes is not merely imprecise, it is the wrong law");
        std::printf("        (Stokes overstates quartz by %.2fx in sand, %.0fx in gravel)\n", r_sand, r_grav);

        check(reynolds(settling_velocity(PHASES[QUARTZ].density, bin_diameter(CLAY)), bin_diameter(CLAY)) < 1.0 &&
              reynolds(settling_velocity(PHASES[QUARTZ].density, d_grav), d_grav) > 1000.0,
              "the bins straddle the creeping-flow and Newton regimes: one correlation cannot be skipped");

        // This assertion used to have no shape clause, and it passed for the wrong
        // reason: every grain was a sphere, so "at equal shape" was vacuously true
        // everywhere. It is the third test in this project caught passing for the
        // wrong reason, and like the other two it was caught by something trying
        // to use the model rather than by reading it. *(Amended 2026-07-10, #13.)*
        bool by_density = true;
        for (int s = 0; s < N_SIZE; ++s)
            for (int p = 0; p < N_PHASE; ++p)
                for (int q = 0; q < N_PHASE; ++q)
                    if (grain_aspect_ratio(p, s) == grain_aspect_ratio(q, s)
                        && PHASES[p].density > PHASES[q].density
                        && free_velocity(p, s) <= free_velocity(q, s))
                        by_density = false;
        check(by_density, "at equal shape, velocity ranks by density within a size class");

        // And shape outranks density, which is the whole of why clay is clay.
        // Kaolinite is denser than feldspar and falls a quarter as fast.
        check(PHASES[KAOLINITE].density > PHASES[FELDSPAR].density
              && free_velocity(KAOLINITE, CLAY) < 0.5 * free_velocity(FELDSPAR, CLAY),
              "but shape outranks density: the denser platelet loses to the lighter lump");
        std::printf("        (kaolinite %.4e m/s at rho 2.60; feldspar %.4e at rho 2.56)\n",
                    free_velocity(KAOLINITE, CLAY), free_velocity(FELDSPAR, CLAY));

        bool size_dominates = true;
        for (int s = 0; s + 1 < N_SIZE; ++s) {
            double hi = 0.0, lo = 1e9;
            for (int p = 0; p < N_PHASE; ++p) {
                hi = std::fmax(hi, free_velocity(p, s));
                lo = std::fmin(lo, free_velocity(p, s + 1));
            }
            if (hi >= lo) size_dominates = false;
        }
        check(size_dominates,
              "across size classes the bands do not even touch: size beats density, always");

        bool between = true;
        for (int p = 0; p < N_PHASE; ++p) {
            if (p == GANGUE) continue;
            for (int s = 0; s < N_SIZE; ++s) {
                const double lo = std::fmin(free_velocity(p, s), free_velocity(GANGUE, s));
                const double hi = std::fmax(free_velocity(p, s), free_velocity(GANGUE, s));
                if (!(composite_velocity(p, s) > lo && composite_velocity(p, s) < hi)) between = false;
            }
        }
        check(between, "a composite grain settles strictly between its phase and its gangue");

        const double vm = free_velocity(MAGNETITE, SAND), vh = free_velocity(HEMATITE, SAND);
        check(std::fabs(vm - vh) / vh < 0.02,
              "magnetite and hematite settle within 2% of each other: no gravity device separates them");

        // ---- 1b. Shape ---------------------------------------------------------
        // The Perrin factor is exact, so it can be checked against a result from a
        // different calculation in a different century: the orientation-averaged
        // Stokes friction of a thin circular disk of radius a is exactly 12*mu*a.
        // Nothing in stokes_shape_factor() knows that. It converges on it anyway.
        bool disk_limit = true;
        for (double p : {1e-3, 1e-4, 1e-5}) {
            // f_tot = f_P * 6 pi mu R, with R = a p^(1/3); so f_tot/(mu a) = 6 pi p^(1/3) / K1.
            const double ftot = 6.0 * M_PI * std::cbrt(p) / stokes_shape_factor(p);
            if (std::fabs(ftot - 12.0) > 1e-2) disk_limit = false;
        }
        check(disk_limit, "the platelet's exact drag converges on the thin-disk result 12*mu*a");

        // A sphere must be a sphere. Every p = 1 phase keeps the velocity it had
        // before shape existed, and that is what makes this change safe.
        check(std::fabs(stokes_shape_factor(1.0) - 1.0) < 1e-15
              && std::fabs(oblate_sphericity(1.0) - 1.0) < 1e-15
              && std::fabs(newton_shape_factor(1.0) - 1.0) < 1e-15
              && std::fabs(drag_coefficient(17.0, 1.0, 1.0) - sphere_drag_coefficient(17.0)) < 1e-15,
              "at p = 1 every shape factor is 1 and the drag law is the sphere's, exactly");

        // Ganser's rescaling is exact in the Stokes limit by construction: the K2
        // that stretches the Newton regime cancels out of the creeping one. What is
        // left over at finite Re is Schiller & Naumann's fitted correction, and it
        // dies with Re -- 4.5e-5 at Re 1e-6, 8e-8 at Re 1e-10 -- rather than being
        // driven to zero by hand.
        {
            const double K1 = stokes_shape_factor(0.1);
            const double K2 = newton_shape_factor(oblate_sphericity(0.1));
            const double near = std::fabs(drag_coefficient(1e-6,  K1, K2) * 1e-6  * K1 / 24.0 - 1.0);
            const double far  = std::fabs(drag_coefficient(1e-10, K1, K2) * 1e-10 * K1 / 24.0 - 1.0);
            check(far < 1e-6 && far < near,
                  "and as Re -> 0 the shaped drag law is exactly 24/(Re*K1): no fit survives there");
        }

        // Nearly all of the 6.98x is volume, not drag. A plate of face diameter d
        // has the volume of a sphere of 0.464 d, and velocity goes as d^2.
        check(std::fabs(stokes_shape_factor(0.1) - 0.6860) < 1e-3
              && std::fabs(free_velocity(QUARTZ, CLAY) / free_velocity(KAOLINITE, CLAY) - 6.977) < 1e-2,
              "a 10:1 plate loses 1.46x to drag and 4.8x to volume, for 6.98x in the clay bin");

        // ---- 1c. Water ---------------------------------------------------------
        // Kell's polynomial is a fit to density. It was never given the temperature
        // of maximum density, and it puts it at 3.983 C.
        check(std::fabs(water_density(273.15) - 999.8395) < 1e-3
              && std::fabs(water_density(293.15) - 998.2071) < 5e-3
              && std::fabs(water_viscosity(293.15) - 1.0016e-3) < 1e-5,
              "the water correlations reproduce published density and viscosity at 0 C and 20 C");

        double rho_max = 0.0, t_max = 0.0;
        for (double t = 0.0; t < 10.0; t += 0.001)
            if (water_density(t + 273.15) > rho_max) { rho_max = water_density(t + 273.15); t_max = t; }
        check(std::fabs(t_max - 3.98) < 0.01,
              "and water is densest at 3.98 C, which nobody fitted and nobody typed in");

        // Substance::temperature stopped being decoration on 2026-07-10.
        {
            Substance cold; cold.temperature = 274.15;   // snowmelt
            Substance warm; warm.temperature = 313.15;   // a hot spring
            const double v_cold = free_velocity(KAOLINITE, CLAY, cold.temperature);
            const double v_warm = free_velocity(KAOLINITE, CLAY, warm.temperature);
            check(v_warm > 2.5 * v_cold,
                  "clay falls more than twice as fast in a hot spring as in a snowmelt creek");
            std::printf("        (kaolinite in clay: %.4e m/s at 1 C, %.4e at 40 C, %.2fx)\n",
                        v_cold, v_warm, v_warm / v_cold);
        }

        // Levigation is the one process in the design that quotes settling times,
        // and it is also the one place Stokes is exact. So the times are not a
        // fact about clay. They are a fact about clay and the depth of your hole.
        // The clay time is now a fact about clay, the depth of your hole, AND the
        // grain's shape. A 2 um kaolinite platelet takes 61 hours to fall through
        // 0.10 m where a 2 um kaolinite sphere -- which does not exist -- takes 9.1.
        // DESIGN.md's table quoted the sphere. *(2026-07-10, #13.)*
        const double DEPTH = 0.10; // m of standing water. AUTHORED: it's a puddle.
        const double d_sand_fine = 62.5e-6, d_silt = 20.0e-6, d_clay = 2.0e-6;
        const double p_clay = PHASES[KAOLINITE].aspect_ratio;
        const double t_sand = DEPTH / stokes_velocity(PHASES[QUARTZ].density, d_sand_fine);
        const double t_silt = DEPTH / stokes_velocity(PHASES[QUARTZ].density, d_silt);
        const double t_clay = DEPTH / stokes_velocity(PHASES[KAOLINITE].density, d_clay, p_clay);
        const double t_sphere = DEPTH / stokes_velocity(PHASES[KAOLINITE].density, d_clay);
        check(reynolds(stokes_velocity(PHASES[QUARTZ].density, d_sand_fine), d_sand_fine) < 1.0,
              "levigation lives entirely in creeping flow, so its settling times are exactly derivable");
        check(t_sand < t_silt && t_silt < t_clay,
              "levigation separates by settling time, and the order is sand, silt, clay");
        check(std::fabs(t_clay / t_sphere - 6.77) < 0.05,
              "and a platelet takes 6.8x as long to fall as the sphere that never existed");
        std::printf("        (0.10 m of water: sand %.0f s | silt %.0f s | clay %.1f h, was %.1f h)\n",
                    t_sand, t_silt, t_clay / 3600.0, t_sphere / 3600.0);
    }

    // ---- 1. Conservation ---------------------------------------------------
    {
        const SeparationResult r = separate(feed, PAN);
        bool cells = true;
        for (int p = 0; p < N_PHASE; ++p)
            for (int s = 0; s < N_SIZE; ++s) {
                if (std::fabs(r.concentrate.freegrain[p][s] + r.tailings.freegrain[p][s] - feed.freegrain[p][s]) > 1e-12) cells = false;
                if (std::fabs(r.concentrate.composite[p][s] + r.tailings.composite[p][s] - feed.composite[p][s]) > 1e-12) cells = false;
            }
        check(cells, "separation conserves every particle population, cell by cell");

        bool phases = true;
        for (int p = 0; p < N_PHASE; ++p)
            if (std::fabs(r.concentrate.phase_mass(p) + r.tailings.phase_mass(p) - feed.phase_mass(p)) > 1e-12) phases = false;
        check(phases, "separation conserves every phase, including gangue riding in composites");

        const Substance c = crush(raw, 0.6);
        bool crush_ok = true;
        for (int p = 0; p < N_PHASE; ++p)
            if (std::fabs(c.phase_mass(p) - raw.phase_mass(p)) > 1e-9) crush_ok = false;
        check(crush_ok, "crushing conserves every phase (breakage moves mass, it does not make it)");

        bool screens_ok = true;
        for (int cut = SAND; cut <= GRAVEL; ++cut) {
            const ScreenResult sr = screen(raw, 0.8, cut);
            if (std::fabs(sr.oversize.total_mass() + sr.undersize.total_mass() - raw.total_mass()) > 1e-12) screens_ok = false;
        }
        check(screens_ok, "screening conserves mass, at either cut");
    }

    // ---- 2. The law, and it only holds on one size class --------------------
    {
        for (int target : {MAGNETITE, HEMATITE}) {
            const auto f = frontier(feed, PAN, target);
            bool mono = true;
            for (size_t i = 1; i < f.size(); ++i) {
                if (f[i].rec > f[i-1].rec + 1e-12) mono = false;
                if (f[i].grade < f[i-1].grade - 1e-12) mono = false;
            }
            check(mono, target == MAGNETITE
                  ? "sized feed: raising the cut trades magnetite recovery for grade, at every step"
                  : "sized feed: and for hematite too, so it is not a property of one phase");
        }

        const auto f = frontier(feed, PAN, MAGNETITE);
        bool buys = true;
        for (const auto& p : f)
            if (p.rec < 0.99 && p.grade <= feed.grade(MAGNETITE)) buys = false;
        check(buys, "any cut that costs recovery buys grade; a cut that costs nothing buys nothing");

        const auto fq = frontier(feed, PAN, QUARTZ);
        bool gangue_falls = true;
        for (size_t i = 1; i < fq.size(); ++i)
            if (fq[i].grade > fq[i-1].grade + 1e-12) gangue_falls = false;
        check(gangue_falls, "the gangue's grade falls as the target's rises: one law, seen from the other side");
    }

    // ---- 3. Why you screen first -------------------------------------------
    // Feed a separator two size classes and the law breaks. The grade curve
    // climbs inside the fines, collapses when the cut leaves them behind, and
    // climbs again inside the sand. This is not a modelling artefact; it is the
    // reason every mill on earth classifies before it concentrates, and the
    // reason panning begins by washing off the mud and picking out the pebbles.
    {
        const Substance two_class = screen(raw, 1.0, GRAVEL).undersize; // fines + sand
        const auto f = frontier(two_class, PAN, MAGNETITE);
        int reversals = 0;
        for (size_t i = 1; i < f.size(); ++i) if (f[i].grade < f[i-1].grade - 1e-9) ++reversals;
        check(reversals > 0, "on a feed spanning two size classes the grade curve is not monotone at all");
        std::printf("        (%d reversals in %zu steps; the law is a within-class law)\n", reversals, f.size() - 1);

        // And with no screening whatsoever, the pan stops being a separator.
        Substance cur = raw;
        for (int n = 0; n < 256; ++n) cur = separate(cur, PAN).concentrate;
        double sand_mass = 0.0, gravel_mass = 0.0;
        for (int p = 0; p < N_PHASE; ++p) {
            sand_mass   += cur.freegrain[p][SAND]   + cur.composite[p][SAND];
            gravel_mass += cur.freegrain[p][GRAVEL] + cur.composite[p][GRAVEL];
        }
        check(sand_mass < 1e-6 * gravel_mass && cur.grade(QUARTZ) > cur.grade(MAGNETITE),
              "an unscreened pan converges on the coarsest bin: it is a pebble-picking machine");
        std::printf("        (unscreened, 256 passes: %.4f kg gravel, %.2e kg sand, quartz grade %.3f)\n",
                    gravel_mass, sand_mass, cur.grade(QUARTZ));
    }

    // ---- 4. The tool moves the curve, and by exactly how much ---------------
    {
        SeparatorParams dull = PAN, sharp = PAN;
        dull.sharpness = HANDS.sharpness; sharp.sharpness = SLUICE.sharpness;
        const auto fd = frontier(feed, dull, MAGNETITE);
        const auto fs = frontier(feed, sharp, MAGNETITE);

        bool dominates = true; int compared = 0;
        for (double rec = 0.05; rec <= 0.95; rec += 0.025) {
            double gd, gs;
            if (grade_at_recovery(fd, rec, gd) && grade_at_recovery(fs, rec, gs)) {
                ++compared;
                if (gs <= gd) dominates = false;
            }
        }
        check(compared >= 20, "the two tools overlap on enough of the recovery range to compare");
        check(dominates, "a sharper cut dominates a duller one at EVERY matched recovery");

        // Drive the cut far above the feed and the logistic becomes a power law:
        // two free phases report to the concentrate in the ratio
        //     (v1 / v2) ^ (1 / sigma).
        // Nothing in that expression is authored except sigma itself. It is the
        // whole of "a better tool moves the curve", and it is arithmetic.
        auto enrichment = [](int a, int b, double sigma) {
            SeparatorParams sp = PAN; sp.sharpness = sigma; sp.cut_velocity = 1e6;
            Substance s; s.freegrain[a][SAND] = 1.0; s.freegrain[b][SAND] = 1.0;
            const Substance c = separate(s, sp).concentrate;
            return c.freegrain[a][SAND] / c.freegrain[b][SAND];
        };
        bool law = true;
        for (const SeparatorParams* sp : {&HANDS, &PAN, &SLUICE}) {
            const double got  = enrichment(MAGNETITE, QUARTZ, sp->sharpness);
            const double want = std::pow(free_velocity(MAGNETITE, SAND) / free_velocity(QUARTZ, SAND), 1.0 / sp->sharpness);
            if (std::fabs(got / want - 1.0) > 1e-5) law = false;
        }
        check(law, "the limiting enrichment of a single stage is (v1/v2)^(1/sigma), derived not chosen");

        const double eh = enrichment(MAGNETITE, QUARTZ, HANDS.sharpness);
        const double ep = enrichment(MAGNETITE, QUARTZ, PAN.sharpness);
        const double es = enrichment(MAGNETITE, QUARTZ, SLUICE.sharpness);
        check(eh < ep && ep < es, "so the tool ladder is an enrichment ladder, and nothing else");
        std::printf("        (magnetite over quartz, one stage: hands %.2fx  pan %.2fx  sluice %.2fx)\n", eh, ep, es);
        std::printf("        (hematite over magnetite, one pan stage: %.4fx — this is the lodestone's reason)\n",
                    enrichment(HEMATITE, MAGNETITE, PAN.sharpness));
    }

    // ---- 5. The liberation ceiling is real, and it is f --------------------
    // Every particle of a fully locked phase is half gangue by mass. No cut, no
    // tool, no number of passes can produce a concentrate richer than that.
    {
        const Substance all_locked = sized(with_magnetite_liberation(0.0));
        bool ceiling = true;
        for (const SeparatorParams* sp : {&HANDS, &PAN, &SLUICE})
            for (double lv = std::log(1e-4); lv <= std::log(3.0); lv += 0.05) {
                SeparatorParams q = *sp; q.cut_velocity = std::exp(lv);
                if (separate(all_locked, q).concentrate.grade(MAGNETITE) > COMPOSITE_TARGET_FRACTION + 1e-9) ceiling = false;
            }
        check(ceiling, "fully locked magnetite can never exceed the composite's own grade");

        const double g03 = peak_grade_over_passes(sized(with_magnetite_liberation(0.3)), PAN, 64, MAGNETITE);
        const double g05 = peak_grade_over_passes(sized(with_magnetite_liberation(0.5)), PAN, 64, MAGNETITE);
        const double g10 = peak_grade_over_passes(sized(with_magnetite_liberation(1.0)), PAN, 64, MAGNETITE);
        check(g03 < g05 && g05 < g10, "liberation orders the best grade any amount of work can reach");
        std::printf("        (best attainable grade: L=0.3 %.3f | L=0.5 %.3f | L=1.0 %.3f)\n", g03, g05, g10);
    }

    // ---- 6. A gravity separator ranks by velocity, and nothing else ---------
    // Re-panning forever does not converge on your target. Within one size class
    // it converges on the densest phase present. Hematite is denser than
    // magnetite, so in the limit the pan throws your magnetite away. This is why
    // the lodestone exists, and it is not a cheat: it reads an axis gravity
    // cannot see.
    {
        Substance cur = feed;
        double best = 0.0; int best_n = 0;
        for (int n = 1; n <= 256; ++n) {
            cur = separate(cur, PAN).concentrate;
            if (cur.grade(MAGNETITE) > best) { best = cur.grade(MAGNETITE); best_n = n; }
        }
        check(best_n > 1 && best_n < 256, "magnetite grade peaks at a finite number of passes, then falls");
        check(cur.grade(HEMATITE) > cur.grade(MAGNETITE),
              "in the limit the concentrate is the fastest-settling phase, not the wanted one");
        check(recovery(feed, cur, MAGNETITE) < 0.05 * recovery(feed, separate(feed, PAN).concentrate, MAGNETITE),
              "cleaning stages spend recovery: the tradeoff reappears across passes");
        std::printf("        (best magnetite grade %.3f at pass %d; at pass 256 magnetite %.3f, hematite %.3f)\n",
                    best, best_n, cur.grade(MAGNETITE), cur.grade(HEMATITE));
    }

    // ---- 7. Over-processing, and what actually causes it -------------------
    // A blow both liberates and over-grinds; the two are one act. DESIGN.md
    // claimed the resulting optimum was "a day-one tradeoff with no arbitrary
    // numbers in it." Measured, that is half right, and the half that is wrong
    // is worth more than the half that is right. The finding survived the move
    // from a density cut to a velocity cut, and got sharper: it now takes a
    // *worse* screen to manufacture the optimum than it did before.
    {
        const Substance locked = weathered_outcrop();

        auto sweep = [&](const Substance& ore, double eff, int cycles,
                         double& best_i, double& best_rec, bool& interior) {
            best_i = -1.0; best_rec = -1.0;
            for (double i = 0.0; i <= 0.901; i += 0.05) {
                const double r = dressed_recovery(ore, i, eff, cycles, MAGNETITE);
                if (r > best_rec) { best_rec = r; best_i = i; }
            }
            interior = (best_i > 1e-9) && (best_i < 0.899);
        };

        double bi, br; bool interior;

        // A good screen removes finished material before it can be reground, so
        // grinding harder is simply better. This is why closed circuits exist.
        sweep(locked, 1.00, 3, bi, br, interior);
        check(!interior && bi > 0.899, "behind a perfect screen, more grinding is always better");
        sweep(locked, 0.75, 3, bi, br, interior);
        check(!interior && bi > 0.899, "behind a good screen (0.75), more grinding is still always better");
        sweep(locked, 0.50, 3, bi, br, interior);
        check(!interior && bi > 0.899, "and behind a mediocre one (0.50) too");

        // Let the screen misplace enough finished material back into the mill and
        // the optimum appears. Its existence, and its location, are set by the
        // screen's efficiency — which is an AUTHORED number. Over-grinding is
        // therefore not free. It is bought.
        sweep(locked, 0.25, 3, bi, br, interior);
        check(interior, "behind a bad screen (0.25), recovery peaks at an interior crush intensity");
        std::printf("        (screen 0.25: recovery peaks %.3f at intensity %.2f)\n", br, bi);

        // The parameter-free half, and it is the one worth teaching: crushing
        // material that is already liberated has nothing to liberate. It only
        // makes fines, and fines settle too slowly to catch. This holds at every
        // screen efficiency, and no number in this project can be tuned to make
        // it false.
        ScoopBuilder pb;
        const double q[N_SIZE]  = {0.21, 0.84, 5.25, 0.70}, ql[N_SIZE] = {1.0, 1.0, 1.0, 1.0};
        const double mg[N_SIZE] = {0.024, 0.096, 0.60, 0.08}, mgl[N_SIZE] = {0.95, 0.95, 0.95, 0.95};
        pb.put(MAGNETITE, mg, mgl); pb.put(QUARTZ, q, ql);
        const Substance placer = pb.build();

        bool always_harmful = true;
        for (double eff : {1.00, 0.75, 0.50}) {
            double prev = 1e9;
            for (double i = 0.0; i <= 0.901; i += 0.1) {
                const double r = dressed_recovery(placer, i, eff, 3, MAGNETITE);
                if (r > prev + 1e-9) always_harmful = false;
                prev = r;
            }
        }
        check(always_harmful, "crushing an already-liberated ore only ever destroys recovery");

        // And over-grinding is not a separate failure mode at all: grade climbs
        // monotonically as recovery is spent. It is the same law, on the size
        // axis instead of the velocity axis. It was implemented once.
        bool grade_climbs = true; double prev_g = -1.0;
        for (double i = 0.0; i <= 0.901; i += 0.05) {
            const double g = separate(dress(locked, i, 0.50, 8), PAN).concentrate.grade(MAGNETITE);
            if (g < prev_g - 1e-9) grade_climbs = false;
            prev_g = g;
        }
        check(grade_climbs, "past the recovery optimum, grinding still buys grade: the law again");

        const double r0 = dressed_recovery(locked, 0.0, 0.50, 3, MAGNETITE);
        sweep(locked, 0.50, 3, bi, br, interior);
        check(br > r0, "crushing a locked ore is worth doing at all");
        std::printf("        (locked outcrop, screen 0.50: recovery %.3f uncrushed -> %.3f at intensity %.2f)\n", r0, br, bi);
    }

    // ---- 8. The bed, and why a gentle hand beats a hard one -----------------
    // separate() washes every particle in the pan. A real pan washes only its
    // skin, because the heavies stratify underneath and a bed cannot be carried
    // off. exposed() is that skin, and it is derived: an exponential (Rouse)
    // concentration profile in settling velocity, normalised to the skin's mass.
    //
    // The consequence is not a small correction. Without a bed, how hard you swirl
    // barely changes the concentrate you can reach at a given recovery — the only
    // decision is when to stop, which a player feels as a timer. With a bed, a
    // gentle hand keeps the magnetite buried while the quartz walks off the top,
    // and patience becomes a strategy the physics rewards. Nobody balanced that.
    {
        const double skin = skin_mass();

        // A skin is part of the pan, and it is not bigger than the pan.
        bool subset = true, capped = true;
        for (double v : {0.010, 0.045, 0.200, 5.0}) {
            const Substance top = exposed(feed, v, skin);
            if (top.total_mass() > skin + 1e-12) capped = false;
            for (int p = 0; p < N_PHASE; ++p)
                for (int s = 0; s < N_SIZE; ++s)
                    if (top.freegrain[p][s] > feed.freegrain[p][s] + 1e-12 ||
                        top.composite[p][s] > feed.composite[p][s] + 1e-12) subset = false;
        }
        check(subset, "the exposed skin is a subset of the pan, cell by cell");
        check(capped, "and it never holds more than the skin's own mass");

        check(exposed(feed, 0.0, skin).total_mass() < 1e-12,
              "still water reaches nothing: with no mixing the whole pan is bed");

        // The skin is what the water sees, so the skin is poor in the heavy stuff.
        // This one inequality is the entire mechanism.
        const Substance top = exposed(feed, 0.045, skin);
        check(top.grade(MAGNETITE) < feed.grade(MAGNETITE),
              "the skin is depleted in the fastest-settling phase: that is what a bed IS");
        std::printf("        (skin %.0f g of %.0f g; magnetite %.3f in the pan, %.3f in the skin)\n",
                    skin * 1000.0, feed.total_mass() * 1000.0, feed.grade(MAGNETITE), top.grade(MAGNETITE));

        // Swirl hard enough and the profile flattens: the skin is a fair sample of
        // the pan, and the bed stops protecting anything. The limit is the old model.
        const Substance stirred = exposed(feed, 1e6, skin);
        check(std::fabs(stirred.grade(MAGNETITE) / feed.grade(MAGNETITE) - 1.0) < 1e-3,
              "swirl hard enough and stratification vanishes: the skin samples the pan");

        // The law itself. Wash a picked pan continuously at three cuts, stop each
        // at the same recovery, and compare the concentrates. Gentle must win at
        // every matched recovery, exactly as a sharper tool does.
        auto wash_to = [&](double cut, double target_rec) {
            Substance pan = feed;
            const double b0 = pan.phase_mass(MAGNETITE);
            SeparatorParams sp = PAN; sp.cut_velocity = cut;
            const double e = 0.05 / 1.50;
            for (int i = 0; i < 400000; ++i) {
                const Substance sk = exposed(pan, cut, skin);
                for (int p = 0; p < N_PHASE; ++p)
                    for (int s = 0; s < N_SIZE; ++s) {
                        pan.freegrain[p][s] -= sk.freegrain[p][s] * (1.0 - std::pow(partition(free_velocity(p, s), sp), e));
                        pan.composite[p][s] -= sk.composite[p][s] * (1.0 - std::pow(partition(composite_velocity(p, s), sp), e));
                    }
                if (pan.phase_mass(MAGNETITE) <= target_rec * b0) break;
            }
            return pan.grade(MAGNETITE);
        };

        bool patience_pays = true; int compared = 0;
        for (double rec : {0.60, 0.40, 0.20}) {
            const double gentle = wash_to(0.030, rec), medium = wash_to(0.045, rec), hard = wash_to(0.070, rec);
            ++compared;
            if (!(gentle > medium && medium > hard)) patience_pays = false;
            std::printf("        (stop at %.0f%% recovery: gentle %.3f > medium %.3f > hard %.3f)\n",
                        100.0 * rec, gentle, medium, hard);
        }
        check(compared == 3 && patience_pays,
              "with a bed, a gentler hand beats a harder one at every matched recovery");
    }

    // ---- 9. Levigation, and the ratchet that is not one ---------------------
    // Era 1's separator shares nothing with Era 0's but the velocity it reads.
    // Everything here is a consequence of `poured(v) = (clear - v*t)/h`, which
    // has no sharpness in it, and none of it can be tuned.
    {
        // The bank, dug for clay rather than for iron.
        Substance dirt;
        {
            const double clayish[N_SIZE] = {0.90, 0.10, 0.00, 0.00};
            const double sandy[N_SIZE]   = {0.04, 0.16, 0.60, 0.20};
            const double silty[N_SIZE]   = {0.10, 0.55, 0.35, 0.00};
            auto put = [&](int p, double frac, const double psd[N_SIZE]) {
                for (int k = 0; k < N_SIZE; ++k) dirt.freegrain[p][k] = 10.0 * frac * psd[k];
            };
            put(KAOLINITE, 0.25, clayish); put(QUARTZ, 0.55, sandy);
            put(FELDSPAR, 0.15, silty);    put(CALCITE, 0.05, silty);
        }

        // A batch decant is a ramp in velocity, so its quartile ratio is 3, for
        // every vessel, every charge and every wait. A pan's imperfection is a
        // number we authored; levigation's is a number we cannot reach.
        bool imperf_three = true;
        for (const Vessel& v : {HOLLOW, Vessel{0.30, 0.15, "pot"}, Vessel{0.02, 0.60, "puddle"}})
            for (double t : {600.0, 3600.0, 14400.0}) {
                const double clear = clear_depth(dirt, v, t);
                if (clear <= 0.0) continue;
                const double p0 = poured_fraction(0.0, clear, v.depth, t);
                if (p0 <= 0.0) continue;
                // v at which the ramp has fallen to 3/4 and 1/4 of its peak.
                const double v75 = 0.25 * clear / t, v25 = 0.75 * clear / t;
                const double r75 = poured_fraction(v75, clear, v.depth, t) / p0;
                const double r25 = poured_fraction(v25, clear, v.depth, t) / p0;
                if (std::fabs(r75 - 0.75) > 1e-9 || std::fabs(r25 - 0.25) > 1e-9) imperf_three = false;
                if (std::fabs(v25 / v75 - 3.0) > 1e-9) imperf_three = false;
            }
        check(imperf_three, "a batch decant's imperfection is exactly 3, and there is no dial on it");

        // Patience buys grade. It cannot buy recovery.
        bool grade_up = true, rec_down = true;
        double pg = -1.0, pr = 2.0;
        for (double t : {60.0, 300.0, 1200.0, 3600.0, 10800.0}) {
            const Substance liq = decant(dirt, HOLLOW, t).liquor;
            if (liq.total_mass() < 1e-9) break;
            const double g = liq.grade(KAOLINITE), r = recovery(dirt, liq, KAOLINITE);
            if (g < pg - 1e-9) grade_up = false;
            if (r > pr + 1e-9) rec_down = false;
            pg = g; pr = r;
        }
        check(grade_up && rec_down, "levigation: waiting longer raises grade and lowers recovery, always");

        const Substance liq1h = decant(dirt, HOLLOW, 3600.0).liquor;
        double coarse = 0.0;
        for (int p = 0; p < N_PHASE; ++p) coarse += liq1h.freegrain[p][SAND] + liq1h.freegrain[p][GRAVEL];
        check(liq1h.grade(KAOLINITE) > 3.0 * dirt.grade(KAOLINITE) && coarse < 1e-9,
              "one hour in a hole rejects every grain of sand and triples the clay grade");

        // Volume buys recovery, and depth buys almost nothing -- which is what the
        // algebra said in the first place.
        //
        // *(This assertion used to read "at equal volume a deep vessel beats a
        // shallow one: the clay is settling too", and demanded r_deep > 3 * r_flat.
        // It measured 19.5% against 4.8% and levigate.h carries a dated correction
        // admitting the algebra had been wrong. The algebra was not wrong. Clay was
        // a sphere, and a spherical kaolinite grain falls 6.98x too fast, so its
        // front travelled far enough in four hours for depth to matter. Give the
        // platelet its real drag and its front moves 1.3 mm in four hours against a
        // 50 mm hollow, depth cancels out of the pour fraction as it always should
        // have, and 3.0x collapses to 1.12x. A correction was published to cover for
        // a defect one file over. Both stay, dated. -- 2026-07-10, #13.)*
        const Vessel flat{0.05, 0.20, "flat"}, deep{0.20, 0.10, "deep"};   // equal volume
        const double r_flat = recovery(dirt, decant(dirt, flat, 14400.0).liquor, KAOLINITE);
        const double r_deep = recovery(dirt, decant(dirt, deep, 14400.0).liquor, KAOLINITE);
        check(std::fabs(flat.volume() - deep.volume()) < 1e-9
              && r_deep > r_flat && r_deep < 1.25 * r_flat,
              "at equal volume depth is nearly free: the clay is barely settling at all");
        std::printf("        (equal %.1f L: deep recovers %.3f, flat %.3f -- was 3.0x, now %.2fx)\n",
                    flat.volume() * 1000, r_deep, r_flat, r_deep / r_flat);

        // A second decant divides the clay bin, because the clay bin is not one
        // velocity class.
        //
        // *(This read "re-decanting the liquor is a tax: it cannot divide one
        // velocity class", and measured +0.0001 grade for -4.5% recovery per pass.
        // That was Era 0's law -- no velocity separator divides one velocity class
        // -- correctly applied to a premise that was false. Clay-sized kaolinite and
        // clay-sized quartz are not one velocity class; they were one velocity class
        // only while both were spheres of the same diameter, at which point nothing
        // but their densities differed and their densities differ by 1.9%. The old
        // claim and its number stay. -- 2026-07-10, #13.)*
        Substance stage = decant(dirt, HOLLOW, 3600.0).liquor;
        const double g0 = stage.grade(KAOLINITE), r0 = recovery(dirt, stage, KAOLINITE);
        for (int i = 0; i < 3; ++i) stage = decant(stage, HOLLOW, 3600.0).liquor;
        const double g3 = stage.grade(KAOLINITE), r3 = recovery(dirt, stage, KAOLINITE);
        check(g3 > g0 + 1e-3 && r3 > 0.90 * r0,
              "re-decanting buys grade cheaply: the clay bin is two velocity classes");
        std::printf("        (3 more decants: grade %.6f -> %.6f, recovery %.4f -> %.4f)\n",
                    g0, g3, r0, r3);

        // The ratchet. It has exactly one rung, and the rung is not made of sigma.
        //
        // *(This read "a pot pinched from unlevigated dirt is a WORSE separator
        // than cupped hands" -- sigma 1.83 against 1.20 -- and it was the whole of
        // what a stony floor was thought to cost. It is false, and it was false
        // because the bridge in `fire.h` had no way to say "this grain never
        // moved". It charged the cost of sheltering to the blur. The exposed sand
        // on a cobbled floor is sorted about as well as on a smooth one; the 42%
        // of it wedged between the stones is not sorted at all. A separator has
        // two misplacements, which `separate.h` learned about screens on
        // 2026-07-09 and this file relearned about floors a day later. The old
        // claim and its number stay. -- 2026-07-10, #10.)*
        const double d_cut = cut_diameter(PAN.cut_velocity);
        const double raw_shelter = sheltered_fraction(grit_diameter(dirt), d_cut);
        check(raw_shelter > 0.40 && fire_pan(dirt).sharpness > WRIST_SHARPNESS,
              "a pot pinched from unlevigated dirt is not a bad pan: it is not a pan");
        std::printf("        (unlevigated pot: grit %.0f um, and %.1f%% of the sand at the cut never moves)\n",
                    grit_diameter(dirt) * 1e6, 100.0 * raw_shelter);

        Vessel ves = HOLLOW; double sig[3] = {0, 0, 0};
        for (int gen = 0; gen < 3; ++gen) {
            const Substance body = decant(dirt, ves, 14400.0).liquor;
            sig[gen] = fire_pan(body).sharpness;
            ves = throw_pot(body);
        }
        check(std::fabs(sig[0] - WRIST_SHARPNESS) < 1e-6
              && std::fabs(sig[1] - sig[0]) < 1e-9 && std::fabs(sig[2] - sig[1]) < 1e-9,
              "and then it stops: every generation's pan is the operator's wrist, to nine places");
        std::printf("        (sigma: gen0 %.9f -> gen1 %.9f -> gen2 %.9f; wrist %.9f)\n",
                    sig[0], sig[1], sig[2], WRIST_SHARPNESS);

        // And the one rung is one minute tall. Sand is the only thing coarse
        // enough to matter, and sand is the first thing to fall out of standing
        // water. What one minute buys is not a sharper pan. It is a pan whose
        // floor has stopped swallowing the feed.
        const Substance cobbed = screen(dirt, HAND_COB).undersize;
        const Substance minute = decant(cobbed, HOLLOW, 60.0).liquor;
        check(sheltered_fraction(grit_diameter(cobbed), d_cut) > 0.30
              && sheltered_fraction(grit_diameter(minute), d_cut) == 0.0
              && fire_pan(minute).sharpness < WRIST_SHARPNESS + 1e-4,
              "one minute of standing water takes the shelter to zero and the blur to nothing");
        std::printf("        (stone-picked dirt shelters %.1f%%; after 60 s, %.1f%% and sigma %.6f)\n",
                    100.0 * sheltered_fraction(grit_diameter(cobbed), d_cut),
                    100.0 * sheltered_fraction(grit_diameter(minute), d_cut),
                    fire_pan(minute).sharpness);

        // And the shelter is not just a number on a panel any more: it moves the
        // separation. A wedged grain stays in the pan whatever its velocity, so a
        // stony floor's concentrate is diluted by fines a working pan rejects.
        // Isolate the effect by comparing the same fired pan with its shelter zeroed:
        // the blur is identical, only the second misplacement differs.
        // *(2026-07-10, #19: the printed fraction became an applied partition.)*
        const SeparatorParams stony_pan = fire_pan(dirt);          // shelter[] filled
        SeparatorParams smooth_pan = stony_pan;                    // same blur, no shelter
        for (int s = 0; s < N_SIZE; ++s) smooth_pan.shelter[s] = 0.0;
        const SeparationResult cs = separate(dirt, stony_pan);
        const SeparationResult cl = separate(dirt, smooth_pan);
        const double conserved = std::fabs(cs.concentrate.total_mass()
                                           + cs.tailings.total_mass() - dirt.total_mass());
        check(conserved < 1e-9
              && recovery(dirt, cl.concentrate, KAOLINITE) < 0.01            // a pan rejects clay
              && recovery(dirt, cs.concentrate, KAOLINITE) > 0.30            // a stony floor keeps it
              && cs.concentrate.grade(QUARTZ) < cl.concentrate.grade(QUARTZ) - 0.05,
              "a stony floor swallows the feed: shelter reports rejected fines to the "
              "concentrate and dilutes its grade, applied now, not just printed");
        std::printf("        (stony pan keeps %.1f%% of the clay a smooth floor rejects; "
                    "quartz grade %.3f vs %.3f)\n",
                    100.0 * recovery(dirt, cs.concentrate, KAOLINITE),
                    cs.concentrate.grade(QUARTZ), cl.concentrate.grade(QUARTZ));

        // And the grade ceiling is not a ceiling, because levigation separates.
        //
        // *(This read "every vessel and every wait lands on the same grade: the
        // dirt's own clay bin", pinned at 0.843, and it was the single result this
        // project's Era 1 was built around: levigation authors nothing, cuts on a
        // velocity ratio of 1.031x, cannot divide the clay bin, and therefore leaves
        // a lodestone-shaped hole that only deflocculation can fill (#15). Every
        // step of that was sound except the 1.031x, which was the ratio between two
        // SPHERES of equal diameter and unequal density -- i.e. it was a fact about
        // an assumption. Kaolinite is a platelet. The ratio is 6.977x, the clay bin
        // divides, and the hole is not a hole. -- 2026-07-10, #13.)*
        double cb = 0.0;
        for (int p = 0; p < N_PHASE; ++p) cb += dirt.freegrain[p][CLAY];
        const double bin_grade = dirt.freegrain[KAOLINITE][CLAY] / cb;
        bool beats_bin = false;
        for (const Vessel& v : {HOLLOW, Vessel{0.30, 0.15, "pot"}, Vessel{0.40, 0.30, "vat"}})
            for (double t : {3600.0, 14400.0}) {
                const Substance liq = decant(dirt, v, t).liquor;
                if (liq.total_mass() > 1e-9 && liq.grade(KAOLINITE) > bin_grade + 0.01) beats_bin = true;
            }
        check(beats_bin, "levigation lifts the clay above the bin it came from: it is a separator");
        std::printf("        (the dirt's own clay bin is %.4f; a 4 h hollow returns %.4f;\n"
                    "         kaolinite/quartz velocity ratio in clay is %.3fx, not 1.03x)\n",
                    bin_grade, decant(dirt, HOLLOW, 14400.0).liquor.grade(KAOLINITE),
                    free_velocity(QUARTZ, CLAY) / free_velocity(KAOLINITE, CLAY));

        // What replaces the ceiling is a curve, which is what every other separator
        // in this project has. Levigation had only a recovery axis until today.
        const Substance liq_hollow = decant(dirt, HOLLOW, 14400.0).liquor;
        const Substance liq_wide   = decant(dirt, Vessel{0.05, 0.40, "wide"}, 14400.0).liquor;
        check(liq_hollow.grade(KAOLINITE) > liq_wide.grade(KAOLINITE)
              && recovery(dirt, liq_wide, KAOLINITE) > recovery(dirt, liq_hollow, KAOLINITE),
              "and grade trades against recovery across vessels: levigation has a curve now");

        // The curve moved OUTWARD. A bigger pot is a better tool by this project's
        // own definition, and it is not throughput: it digs no more dirt, it loses
        // less of the clay in what it has. Throughput would be a bigger shovel, and
        // a bigger shovel moves nothing. That distinction is one the README got
        // wrong for an hour.
        //
        // *(The assertion used to be "same grade, an order more recovery", which
        // was only sayable while every vessel landed on 0.843. Now that grade moves,
        // more recovery at less grade is a slide ALONG the curve, not a move of it,
        // and the honest test is the one this project's own README specifies:
        // better recovery at a MATCHED grade. The pot wins at every matched grade,
        // so the finding stands and its old proof does not. -- 2026-07-10, #13.)*
        const Vessel pot{0.30, 0.15, "pot"};
        auto lev_frontier = [&](const Vessel& v) {
            std::vector<Point> f;
            for (double t : {60.0, 300.0, 900.0, 3600.0, 14400.0, 57600.0, 230400.0, 921600.0}) {
                const Substance liq = decant(dirt, v, t).liquor;
                if (liq.total_mass() < 1e-9) continue;
                f.push_back({levigation_cut(clear_depth(dirt, v, t), t),
                             recovery(dirt, liq, KAOLINITE), liq.grade(KAOLINITE)});
            }
            return f;
        };
        const auto fh = lev_frontier(HOLLOW), fp = lev_frontier(pot);
        bool pot_dominates = !fh.empty() && !fp.empty();
        double matched = -1.0;
        for (const auto& a : fh) {           // for every grade the hollow can reach...
            double best = -1.0;
            for (const auto& b : fp) if (b.grade >= a.grade - 1e-9) best = std::fmax(best, b.rec);
            if (best <= a.rec) pot_dominates = false;   // ...the pot recovers more of it
            if (std::fabs(a.grade - fh.back().grade) < 1e-12) matched = best;
        }
        check(pot_dominates,
              "a bigger pot moves the curve outward: more recovery at every matched grade");
        std::printf("        (at the hollow's purest grade %.4f: hollow recovers %.4f, pot %.4f)\n",
                    fh.back().grade, fh.back().rec, matched);

        Substance big = dirt; big.add(dirt);   // a bigger shovel: twice the charge
        const double rec_p = recovery(dirt, decant(dirt, pot, 14400.0).liquor, KAOLINITE);
        check(recovery(big, decant(big, pot, 14400.0).liquor, KAOLINITE) < rec_p,
              "and a bigger shovel moves nothing outward: that is what throughput is");
        std::printf("        (at 4 h: pot recovers %.4f, pot with a double charge %.4f)\n",
                    rec_p, recovery(big, decant(big, pot, 14400.0).liquor, KAOLINITE));

        // Flocculation: the recovery half of levigation, and a separation a settling
        // cut cannot make. Deflocculated levigation (everything above) buys grade for
        // patience and leaves the clay dispersed. Flocculate the poured liquor and the
        // clay drops in minutes -- and because quartz keeps its charge and will not
        // floc, the solid it hands back is purer than the liquor it settled from,
        // dividing a single velocity class by surface charge. #13 split that class by
        // shape; this splits it by a variable velocity cannot read.
        // *(2026-07-10, #15. The floc size is authored, #29.)*
        const Substance liquor = decant(dirt, HOLLOW, 3600.0).liquor;   // a clay liquor
        const FloccResult fl = flocculate(liquor, HOLLOW, 300.0);       // stand five minutes
        double defloc_kaol = 0.0;   // what plain deflocculated standing drops in 5 min
        for (int s = 0; s < N_SIZE; ++s)
            defloc_kaol += liquor.freegrain[KAOLINITE][s]
                * settled_fraction(free_velocity(KAOLINITE, s, liquor.temperature), HOLLOW.depth, 300.0);
        const double floc_rec   = recovery(liquor, fl.recovered, KAOLINITE);
        const double defloc_rec = liquor.phase_mass(KAOLINITE) > 0.0
            ? defloc_kaol / liquor.phase_mass(KAOLINITE) : 0.0;
        const double floc_conserved = std::fabs(fl.recovered.total_mass()
            + fl.supernatant.total_mass() - liquor.total_mass());
        check(floc_conserved < 1e-9
              && floc_rec > 0.90                              // the clay is down in minutes
              && floc_rec > 100.0 * defloc_rec               // where dispersed it barely moves
              && fl.recovered.grade(KAOLINITE) > liquor.grade(KAOLINITE) + 1e-3,  // purer than feed
              "flocculation is levigation's recovery half: it drops the clay in minutes, and "
              "because quartz will not floc it hands back a solid purer than the liquor");
        std::printf("        (5 min: flocculated recovers %.3f of the clay vs %.5f dispersed; "
                    "solid grade %.4f from a %.4f liquor)\n",
                    floc_rec, defloc_rec, fl.recovered.grade(KAOLINITE), liquor.grade(KAOLINITE));
    }

    // ---- 10. The bridge under the ratchet, and it is derived now ------------
    //
    // Issue #10 asked whether `sigma^2 = sigma_wrist^2 + (d_grit/h_skin)^2` had
    // the right functional form, and warned that a `sqrt(d/h)` would move every
    // number by 35x and might restore the spiral the staircase replaced. It is
    // the wrong form. The right one is the log law over a rough bed; it is
    // super-linear rather than sub-linear; and the spiral stays dead. Nothing
    // below is asserted. Every line of it is checked against the thing it claims
    // to be -- a numeric mixture, a numeric integral, a numeric derivative.
    {
        std::printf("\n  -- 10. the grit -> sharpness bridge, checked rather than believed --\n");
        const double d_cut = cut_diameter(PAN.cut_velocity);

        check(std::fabs(settling_velocity(PHASES[QUARTZ].density, d_cut) - PAN.cut_velocity)
                  < 1e-9 * PAN.cut_velocity
              && d_cut > SIZE_BOUNDS[SAND][0] && d_cut < SIZE_BOUNDS[SAND][1],
              "the grain the pan decides about is derived from the cut, and it lands in sand");
        std::printf("        (quartz falls at the %.3f m/s cut when it is %.1f um across; Re %.1f)\n",
                    PAN.cut_velocity, d_cut * 1e6, reynolds(PAN.cut_velocity, d_cut));

        // The quadrature coefficient. Mix a logistic over Gaussian jitter in its
        // centre, then read the result's quartiles -- which is exactly how this
        // project defines sigma. The answer is not 1, which is what fire.h used.
        auto mixed = [](double x, double sigma, double j) {
            const int N = 2001; const double lo = -8 * j, dx = 16 * j / N;
            double num = 0.0, den = 0.0;
            for (int i = 0; i < N; ++i) {
                const double u = lo + (i + 0.5) * dx;
                const double w = std::exp(-0.5 * (u / j) * (u / j));
                num += w / (1.0 + std::exp(-(x - u) / sigma)); den += w;
            }
            return num / den;
        };
        auto reported = [&](double sigma, double j) {
            auto quantile = [&](double target) {
                double lo = -40.0, hi = 40.0;
                for (int i = 0; i < 100; ++i) {
                    const double m = 0.5 * (lo + hi);
                    if (mixed(m, sigma, j) < target) lo = m; else hi = m;
                }
                return 0.5 * (lo + hi);
            };
            return (quantile(0.75) - quantile(0.25)) / (2.0 * std::log(3.0));
        };
        const double jj = 0.05, rep = reported(WRIST_SHARPNESS, jj);
        const double c_measured = std::sqrt(rep * rep - WRIST_SHARPNESS * WRIST_SHARPNESS) / jj;
        check(std::fabs(c_measured - QUADRATURE) < 5e-3,
              "what adds in quadrature is j/sqrt(2 ln 3), not j: 0.6746, measured to three places");
        std::printf("        (measured %.6f, derived %.6f; the old bridge used %.6f)\n",
                    c_measured, QUADRATURE, 1.0);

        // The closed form is the leading term of an integral. Check it against
        // the integral, everywhere a pot's floor is allowed to be rough.
        auto exact_spread = [](double dg, double dp) {
            const int N = 200001; const double z0 = dg / NIKURADSE;
            double s = 0.0, s2 = 0.0; int n = 0;
            for (int i = 0; i < N; ++i) {
                const double zc = 0.5 * dp + (-0.5 + (i + 0.5) / N) * dg;
                if (zc <= z0) continue;
                const double y = std::log(std::log(zc / z0));
                s += y; s2 += y * y; ++n;
            }
            return std::sqrt(s2 / n - (s / n) * (s / n));
        };
        bool agrees = true;
        for (double dg : {1e-7, 1e-6, 1e-5, 1e-4, d_cut / 3.0})
            agrees = agrees && std::fabs(roughness_spread(dg, d_cut) / exact_spread(dg, d_cut) - 1.0) < 0.05;
        check(agrees, "the closed-form spread is the integral it claims to be, to 5% out to d_cut/3");

        // #10 feared an exponent of 0.5. The log law gives 1 + 1/ln(15 d_cut/d_grit).
        bool superlinear = true;
        for (double dg : {1e-7, 1e-6, 1e-5}) {
            const double e = (std::log(roughness_spread(dg * 1.01, d_cut))
                            - std::log(roughness_spread(dg / 1.01, d_cut))) / (2.0 * std::log(1.01));
            superlinear = superlinear && e > 1.0
                && std::fabs(e - (1.0 + 1.0 / std::log(15.0 * d_cut / dg))) < 2e-3;
        }
        check(superlinear, "the exponent is 1 + 1/ln(15 d_cut/d_grit): above 1, and never 0.5");

        // The claim that replaced "267 um of grit blurs the pan by 10%".
        double worst = 0.0;
        for (double dg = 1e-9; dg < 1.0; dg *= 1.2)
            worst = std::fmax(worst, std::hypot(WRIST_SHARPNESS,
                                                QUADRATURE * roughness_spread(dg, d_cut)));
        check(worst < 0.5686 && worst > 0.5684,
              "roughness cannot blur the pan past 3.4%: a 10% blur is not purchasable at any grit");
        std::printf("        (sigma tops out at %.6f as the grit approaches the cut grain; 10%% needs 0.605)\n",
                    worst);

        // Sheltering is the other misplacement, and it is where a stony pot's
        // cost actually lives.
        check(sheltered_fraction(bin_diameter(CLAY), d_cut) == 0.0
              && sheltered_fraction(0.5 * d_cut, d_cut) == 0.0
              && sheltered_fraction(d_cut, d_cut) > 0.03
              && sheltered_fraction(5.0 * d_cut, d_cut) > 0.40,
              "shelter is zero under any pot levigation can throw, and 43% under a stony one");

        // What the old bridge got wrong, at the coarsest grit a decant can carry.
        const double authored = bin_diameter(CLAY) / skin_depth();
        const double derived  = QUADRATURE * roughness_spread(bin_diameter(CLAY), d_cut);
        check(authored / derived > 8.0 && authored / derived < 10.0 && derived < 1e-4,
              "the old bridge overstated the blur 8.85x, and the blur was already nothing");
        std::printf("        (authored %.3e, derived %.3e, against a wrist of %.2f)\n",
                    authored, derived, WRIST_SHARPNESS);
    }

    // ---- 11. the lodestone: a force balance on an orthogonal axis ----------
    //
    // The magnet reads magnetic_susceptibility and nothing else. The claims here
    // are that it lifts magnetite and only magnetite; that it does this by a
    // volume-independent force balance; that its authored strength does not
    // matter across two orders of magnitude; that it splits the one pair no pan
    // can (magnetite from hematite); and that a locked grain still caps its grade.
    {
        // (a) magnetite lifts, no other ORE mineral does. IRON is excluded: it is
        // a product, not something you pan, and a magnet grabs a bloom -- asserted
        // just below so the exclusion is not hiding a bug.
        bool only_magnetite = lift_number(PHASES[MAGNETITE].magnetic_susceptibility,
                                          PHASES[MAGNETITE].density) > 1.0;
        for (int p = 0; p < N_PHASE; ++p)
            if (p != MAGNETITE && p != IRON)
                only_magnetite = only_magnetite
                    && lift_number(PHASES[p].magnetic_susceptibility, PHASES[p].density) < 1.0;
        check(only_magnetite,
              "the lodestone lifts magnetite and no other ore mineral");
        check(lift_number(PHASES[IRON].magnetic_susceptibility, PHASES[IRON].density) > 1.0,
              "and it lifts metallic iron too: a magnet sticks to a bloom");

        // (b) a diamagnetic grain is repelled, exactly, not feebly attracted.
        check(magnetic_partition(lift_number(PHASES[QUARTZ].magnetic_susceptibility,
                                             PHASES[QUARTZ].density)) == 0.0
              && magnetic_partition(lift_number(PHASES[CALCITE].magnetic_susceptibility,
                                                PHASES[CALCITE].density)) == 0.0,
              "quartz and calcite are diamagnetic: the stone pushes them away, partition is 0");

        // (c) the gap that makes the magnet's strength irrelevant: magnetite's
        // lift number is more than 1000x the next-strongest phase's. This is why
        // any reach in a wide band lifts magnetite alone.
        double L_mag = lift_number(PHASES[MAGNETITE].magnetic_susceptibility, PHASES[MAGNETITE].density);
        double L_next = 0.0;
        for (int p = 0; p < N_PHASE; ++p)
            if (p != MAGNETITE && p != IRON) // IRON is a product, not an ore gangue
                L_next = std::fmax(L_next,
                    lift_number(PHASES[p].magnetic_susceptibility, PHASES[p].density));
        check(L_mag / L_next > 1000.0,
              "magnetite's lift number is >1000x every other phase: the gap that frees the magnet from tuning");
        std::printf("        (magnetite L = %.1f, next phase L = %.4f, ratio %.0fx)\n",
                    L_mag, L_next, L_mag / L_next);

        // (d) the money test: the one pair no pan can separate. Equal masses of
        // free magnetite and free hematite, in the same size bin, so they settle
        // within 2% and gravity is degenerate on them. The lodestone splits them.
        Substance pair;
        pair.freegrain[MAGNETITE][SAND] = 1.0;
        pair.freegrain[HEMATITE][SAND]  = 1.0;
        const SeparationResult mag = lodestone(pair);
        check(mag.concentrate.grade(MAGNETITE) > 0.99
              && recovery(pair, mag.concentrate, MAGNETITE) > 0.99
              && recovery(pair, mag.concentrate, HEMATITE) < 0.01,
              "the lodestone splits magnetite from hematite, which no pan can: an orthogonal axis");
        std::printf("        (magnetite/hematite: pan enrichment 1.034x per stage, lodestone grade %.4f in one)\n",
                    mag.concentrate.grade(MAGNETITE));

        // (e) the strength of the stone does not matter. A panned concentrate,
        // cleaned by magnets spanning 14x..140x the threshold reach and sharpness
        // 0.2..0.5, lands at the same grade and recovery to within 1%.
        Substance conc = feed;
        for (int i = 0; i < 40; ++i) conc = separate(conc, PAN).concentrate;
        double g_lo = 1.0, g_hi = 0.0, r_lo = 1.0;
        for (double reach : {0.3, 0.9, 3.0})
            for (double sigma : {0.2, 0.35, 0.5}) {
                const SeparationResult r = lodestone(conc, reach, sigma);
                g_lo = std::fmin(g_lo, r.concentrate.grade(MAGNETITE));
                g_hi = std::fmax(g_hi, r.concentrate.grade(MAGNETITE));
                r_lo = std::fmin(r_lo, recovery(conc, r.concentrate, MAGNETITE));
            }
        check(g_hi - g_lo < 0.01 && r_lo > 0.98,
              "the magnet's authored strength does not matter across 10x reach and 2.5x sharpness");
        std::printf("        (grade held in [%.4f, %.4f], recovery >= %.4f, on a feed of %.3f magnetite)\n",
                    g_lo, g_hi, r_lo, conc.grade(MAGNETITE));

        // (f) the grade ceiling is liberation, not the magnet. A fully locked
        // magnetite feed reports every grain to the stone -- gangue and all --
        // and the concentrate grade is exactly the composite fraction, 0.5.
        Substance locked;
        locked.composite[MAGNETITE][SAND] = 1.0;
        const SeparationResult lk = lodestone(locked);
        check(std::fabs(lk.concentrate.grade(MAGNETITE) - COMPOSITE_TARGET_FRACTION) < 1e-9
              && recovery(locked, lk.concentrate, MAGNETITE) > 0.99,
              "a fully locked feed: the stone grabs it all and the grade is the composite's, not 1.0");

        // (g) mass is conserved by the pass, per phase.
        bool conserved = true;
        const SeparationResult cs = lodestone(feed);
        for (int p = 0; p < N_PHASE; ++p)
            conserved = conserved && std::fabs(
                cs.concentrate.phase_mass(p) + cs.tailings.phase_mass(p) - feed.phase_mass(p)) < 1e-9;
        check(conserved, "the lodestone conserves every phase across the pass");
    }

    // ---- 12. the bloomery: slag falls out and the ledger balances ----------
    //
    // The finding is a wall, and it is derived from two atomic weights: with no
    // flux, silica leaves only as fayalite, dragging 1.859 kg of iron per kg of
    // silica. A charge yields metal only above Fe/SiO2 = 1.859 -- which panned
    // sand never reaches and the lodestone always does. That makes smelt
    // downstream of the magnet, not of nothing.
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };

        // (a) the wall constant is 2*M(Fe)/M(SiO2), not a chosen number.
        check(std::fabs(FE_PER_SIO2 - 1.8589) < 1e-3,
              "the fayalite wall is 1.859 kg Fe per kg silica: two atomic weights, no author");

        // (b) pure magnetite smelts; river sand cannot, because its iron is
        // outnumbered by its own silica.
        Substance pure_mag; pure_mag.freegrain[MAGNETITE][SAND] = 1.0;
        const BloomResult bm = bloomery(pure_mag, charcoal(1.0));
        check(bm.bloom_iron > 0.7 && bm.bloom.grade(IRON) > 0.999,
              "pure magnetite yields a bloom of metallic iron");
        check(iron_to_silica(raw) < FE_PER_SIO2 && bloomery(raw, charcoal(5.0)).bloom_iron == 0.0,
              "raw river sand yields no bloom: its silica eats its iron");
        std::printf("        (river sand Fe/SiO2 = %.3f, wall %.3f -- short by %.1fx)\n",
                    iron_to_silica(raw), FE_PER_SIO2, FE_PER_SIO2 / iron_to_silica(raw));

        // (c) the threshold is sharp. Magnetite (Fe 0.724) plus quartz: the ratio
        // crosses 1.859 at a magnetite:quartz mass of 2.569. Straddle it.
        auto ore_at = [](double mag, double qz) {
            Substance o; o.freegrain[MAGNETITE][SAND] = mag; o.freegrain[QUARTZ][SAND] = qz; return o;
        };
        check(bloomery(ore_at(2.7, 1.0), charcoal(2.0)).bloom_iron > 0.0
              && bloomery(ore_at(2.4, 1.0), charcoal(2.0)).bloom_iron == 0.0,
              "the wall is a threshold: 2.7:1 magnetite:quartz smelts, 2.4:1 does not");

        // (d) the chain, and what gates it. Raw sand cannot smelt. The lodestone
        // alone lands close but SHORT -- locked magnetite drags its own quartz to
        // the stone -- so the real gate is LIBERATION: crush to free the magnetite
        // from its gangue, and only then does the magnet reject the silica and the
        // charge clear the wall. Smelt is downstream of the magnet AND the crush.
        Substance panned = raw;
        for (int i = 0; i < 60; ++i) panned = separate(panned, PAN).concentrate;
        const double one_pass = iron_to_silica(lodestone(panned).concentrate);
        Substance liberated = panned;
        for (int i = 0; i < 8; ++i) liberated = crush(liberated, 0.5);
        const Substance cleaned = lodestone(liberated).concentrate;
        check(bloomery(raw, charcoal(5.0)).bloom_iron == 0.0
              && iron_to_silica(cleaned) > FE_PER_SIO2
              && bloomery(cleaned, charcoal(5.0)).bloom_iron > 0.0,
              "dig -> pan -> crush -> lodestone -> bloom: liberation is the gate the furnace needs");
        std::printf("        (lodestone alone: Fe/SiO2 = %.2f, short of %.2f; after crushing: %.2f -> bloom)\n",
                    one_pass, FE_PER_SIO2, iron_to_silica(cleaned));

        // (e) the ledger balances: every element in ore+charcoal leaves in exactly
        // one of bloom, slag, gas.
        const Substance ore = ore_at(3.0, 1.0);
        const Substance fuel = charcoal(2.0);
        const BloomResult br = bloomery(ore, fuel);
        double in_ore[N_ELEM], in_fuel[N_ELEM], bloom_el[N_ELEM];
        assay_elements(ore, in_ore); assay_elements(fuel, in_fuel);
        assay_elements(br.bloom, bloom_el);
        bool balanced = true;
        for (int e = 0; e < N_ELEM; ++e)
            balanced = balanced && std::fabs((in_ore[e] + in_fuel[e])
                                             - (bloom_el[e] + br.slag[e] + br.gas[e])) < 1e-9;
        check(balanced, "the ledger balances: ore + charcoal = bloom + slag + gas, element by element");

        // (f) a cold furnace reduces nothing; all the iron stays in the slag.
        const BloomResult cold = bloomery(pure_mag, charcoal(1.0), false);
        check(cold.bloom_iron == 0.0 && cold.slag[EL_FE] > 0.7,
              "an unlit bloomery makes only slag: reduction needs the reducing envelope");

        // (g) the carbon gate: starve the charcoal and less iron reduces.
        const double fed  = bloomery(pure_mag, charcoal(1.0)).bloom_iron;
        const double lean = bloomery(pure_mag, charcoal(0.1)).bloom_iron;
        check(lean > 0.0 && lean < fed && std::fabs(lean - 0.1 / CARBON_PER_FE) < 1e-9,
              "too little charcoal reduces too little iron: the bloom is carbon-limited");
    }

    // ---- 13. the forge: a spongy bloom becomes a bar, and cannot be cleaned --
    //
    // The first DEFORMATION process. A bloom is a sponge with slag in its pores;
    // hammering closes the pores and squeezes the slag out -- but only through
    // pores that still connect. Below the connectivity floor the trapped slag has
    // no path out, so a wrought bar always carries stringers, and the residual is
    // set by geometry, not by how long you hammer. The fayalite wall's twin. And
    // the grade/recovery law pays out a fourth time: every heat that drains slag
    // also scales iron away, so a cleaner bar is a smaller one.
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };
        auto ore_at = [](double mag, double qz) {
            Substance o; o.freegrain[MAGNETITE][SAND] = mag; o.freegrain[QUARTZ][SAND] = qz; return o;
        };
        // A real smelt with silica in the charge: it clears the wall, so there is a
        // bloom, and it leaves fayalite slag for the pores to hold.
        const BloomResult fired = bloomery(ore_at(3.0, 1.0), charcoal(2.0));
        const Bloom green = pull_bloom(fired);

        // (a) the furnace does not hand you metal. A green bloom carries slag.
        check(green.iron > 0.0 && slag_mass(green.slag) > 0.0,
              "a green bloom is a sponge: iron welded around slag-filled pores, not clean metal");

        // (b) the floor is set by connectivity, not effort. With no scale loss the
        // iron is fixed, so the floor is a true fixed point: blooms that start with
        // very different slag converge to the SAME residual, and hammering past
        // saturation changes nothing.
        const Bloom loose = pull_bloom(fired, 0.45); // porosity 0.55, slag-rich
        const Bloom dense = pull_bloom(fired, 0.70); // porosity 0.30, slag-lean
        const Bar l40 = consolidate(loose, 40, SLAG_EXPULSION, 0.0);
        const Bar d40 = consolidate(dense, 40, SLAG_EXPULSION, 0.0);
        const Bar l80 = consolidate(loose, 80, SLAG_EXPULSION, 0.0);
        check(slag_mass(loose.slag) > 1.5 * slag_mass(dense.slag)
              && std::fabs(slag_fraction(l40) - slag_fraction(d40)) < 1e-6
              && std::fabs(slag_fraction(l40) - slag_fraction(l80)) < 1e-9
              && slag_fraction(l80) > 0.0,
              "the slag floor is connectivity, not effort: different blooms, same residual, and it never reaches zero");

        // (c) purity costs yield: more heats drive the slag down AND the metal down.
        // The pan's law, on the forge's axis.
        const Bar b3  = consolidate(green, 3);
        const Bar b8  = consolidate(green, 8);
        check(slag_fraction(b8) < slag_fraction(b3) && yield(green, b8) < yield(green, b3),
              "a cleaner bar is a smaller bar: every heat expels slag and scales iron away");

        // (d) you cannot hammer a bar clean. The floor is positive at every effort.
        bool always_dirty = true;
        for (int h : {5, 20, 100, 500})
            always_dirty = always_dirty && slag_fraction(consolidate(green, h, SLAG_EXPULSION, 0.0)) > 1e-3;
        check(always_dirty, "wrought iron is wrought: no number of heats produces slag-free metal");

        // (e) the ledger balances element by element: green iron + slag = bar iron
        // + residual slag + scale + drained.
        const Bar bar = consolidate(green, 8);
        bool balanced = true;
        for (int e = 0; e < N_ELEM; ++e) {
            const double gin  = (e == EL_FE ? green.iron : 0.0) + green.slag[e] + green.drained[e];
            const double bout = (e == EL_FE ? bar.iron : 0.0) + bar.slag[e] + bar.scale[e] + bar.drained[e];
            balanced = balanced && std::fabs(gin - bout) < 1e-12;
        }
        check(balanced, "the forge ledger balances: green bloom = bar + scale + drained slag, element by element");

        // (f) scale is iron and nothing else -- it is the metal's own surface, not
        // its slag, that oxidizes and flakes off.
        bool scale_is_iron = true;
        for (int e = 0; e < N_ELEM; ++e)
            if (e != EL_FE) scale_is_iron = scale_is_iron && bar.scale[e] == 0.0;
        check(scale_is_iron && bar.scale[EL_FE] > 0.0,
              "hammer scale is iron: the loss to purity is paid in metal, not in slag");

        const double green_frac = slag_mass(green.slag) / (green.iron + slag_mass(green.slag));
        std::printf("        (green bloom %.1f%% slag; ", 100.0 * green_frac);
        for (int h : {2, 5, 10}) {
            const Bar bh = consolidate(green, h);
            std::printf("%d heats -> %.2f%% slag at %.0f%% yield%s", h,
                        100.0 * slag_fraction(bh), 100.0 * yield(green, bh), h == 10 ? ")\n" : ", ");
        }
    }

    // ---- 14. Shaping: form is free, hardness is only cold --------------------
    // The forge's second half. A consolidated bar is drawn to a form; the same
    // strain gives that form at any heat, but only strain worked in cold (below
    // the recrystallization floor) hardens it. Two ends of the same heat.
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };
        auto ore_at = [](double mag, double qz) {
            Substance o; o.freegrain[MAGNETITE][SAND] = mag; o.freegrain[QUARTZ][SAND] = qz; return o;
        };
        const Bloom green = pull_bloom(bloomery(ore_at(3.0, 1.0), charcoal(2.0)));
        const Bar   bar   = consolidate(green, 6);

        const double HOT  = RECRYST_T + 500.0; // welding heat, well above the floor
        const double COLD = RECRYST_T - 400.0; // black bar, well below it

        // Same schedule of reductions, once hot and once cold.
        Tool hot = stock(bar), cold = stock(bar);
        for (int i = 0; i < 4; ++i) { draw(hot, 0.5, HOT); draw(cold, 0.5, COLD); }

        // (a) form is free at any heat: identical reductions give identical
        // elongation whether the metal was hot or cold.
        check(std::fabs(hot.elongation - cold.elongation) < 1e-9 && hot.elongation > 1.0,
              "form is free at any heat: the same reduction draws the same length hot or cold");

        // (b) hardness is only cold: the hot piece stays annealed-soft; the cold
        // one, worked to the same shape, comes out much harder.
        check(std::fabs(hardness(hot) - H_ANNEALED) < 1e-9
              && hardness(cold) > hardness(hot) + 50.0,
              "hardness is only cold: hot work leaves the metal soft, cold work hardens it");

        // (c) a draw conserves the metal exactly and carries the stringers along
        // -- shaping removes no slag, it only draws it out along the grain.
        bool conserved = std::fabs(cold.metal - bar.iron) < 1e-12;
        for (int e = 0; e < N_ELEM; ++e)
            conserved = conserved && std::fabs(cold.slag[e] - bar.slag[e]) < 1e-12;
        check(conserved, "shaping is pure deformation: the metal and its stringers are conserved, only drawn out");

        // (d) hardening is monotone in cold strain and saturates -- dislocation
        // density has a ceiling, so the last bites of cold work buy almost nothing.
        Tool a = stock(bar), b = stock(bar), c = stock(bar);
        draw(a, 0.5, COLD);                    // one cold bite
        for (int i = 0; i < 4; ++i) draw(b, 0.5, COLD);   // four
        for (int i = 0; i < 20; ++i) draw(c, 0.5, COLD);  // twenty
        check(hardness(b) > hardness(a) && hardness(c) > hardness(b)
              && (hardness(c) - hardness(b)) < 0.1 * (hardness(b) - hardness(a)),
              "cold hardening is monotone and saturating: it climbs, then the anvil stops paying");

        // (e) a single draw obeys volume conservation exactly: reduce the section
        // by r and the length grows by 1/(1-r).
        Tool one = stock(bar);
        draw(one, 0.6, HOT);
        check(std::fabs(one.elongation - 1.0 / (1.0 - 0.6)) < 1e-12,
              "a draw conserves volume: section down by r, length up by 1/(1-r)");

        std::printf("        (4x r=0.5: elong %.1fx both; hot %.0f HB (soft), cold %.0f HB (hard))\n",
                    hot.elongation, hardness(hot), hardness(cold));
    }

    // ---- 15. Red-short: the poison only the hammer can find -------------------
    // Sulfur from pyrite dissolves into the bloom and no Era-1 separator can pull
    // it back out; it rides the metal, invisible, to the anvil. There, above the
    // Fe-FeS eutectic (~988 C), it wets the grain boundaries and the bar cracks
    // under hot work. Cold, it holds -- but cold it will not flow and it hardens.
    // The contaminant no pan or magnet could see decides whether the metal forges.
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };
        auto ore = [](double mag, double qz, double py) {
            Substance o;
            o.freegrain[MAGNETITE][SAND] = mag;
            o.freegrain[QUARTZ][SAND]    = qz;
            if (py > 0.0) o.freegrain[PYRITE][SAND] = py;
            return o;
        };
        const Substance dirty = ore(5.0, 0.5, 0.15); // pyritic charge
        const Substance clean = ore(5.0, 0.5, 0.0);  // same iron, no sulfur

        const BloomResult fd = bloomery(dirty, charcoal(2.0));
        const BloomResult fc = bloomery(clean, charcoal(2.0));

        // (a) sulfur reaches the metal from pyritic ore, and none from clean ore.
        // The partition is real: the acidic slag does not take it all.
        check(fd.bloom_sulfur > 0.0 && fc.bloom_sulfur == 0.0,
              "sulfur dissolves into the bloom: a pyritic charge seeds the metal, a clean one does not");

        // (b) the sulfur ledger balances: what pyrite brought in leaves as metal
        // sulfur plus slag sulfur, nothing lost.
        double in_ore[N_ELEM], in_fuel[N_ELEM];
        assay_elements(dirty, in_ore); assay_elements(charcoal(2.0), in_fuel);
        const double s_in = in_ore[EL_S] + in_fuel[EL_S];
        check(s_in > 0.0 && std::fabs(s_in - (fd.bloom_sulfur + fd.slag[EL_S])) < 1e-12,
              "the sulfur ledger balances: charge S = metal S + slag S");

        // Consolidate both to bars; the dissolved sulfur survives the fire (scale
        // is iron, not sulfur), so it is still there at the anvil.
        const Bar bd = consolidate(pull_bloom(fd), 6);
        const Bar bc = consolidate(pull_bloom(fc), 6);
        check(std::fabs(bd.sulfur - fd.bloom_sulfur) < 1e-12 && bd.sulfur > 0.0,
              "sulfur survives consolidation: the fire scales off iron, not the dissolved poison");

        const double FORGE = EUTECTIC_T + 250.0; // a true forging heat, above the eutectic
        const double COLD  = RECRYST_T - 400.0;  // a black bar, below eutectic and recryst both

        // (c) worked hot, the dirty bar red-shorts while the clean one stays whole.
        Tool dh = stock(bd), ch = stock(bc);
        for (int i = 0; i < 6; ++i) { draw(dh, 0.5, FORGE); draw(ch, 0.5, FORGE); }
        check(soundness(ch) == 1.0 && soundness(dh) < 0.5,
              "red-short is a hot failure: clean iron forges whole, sulfurous iron cracks apart");

        // (d) the SAME dirty bar worked COLD stays perfectly sound -- below the
        // eutectic the sulfide film is solid and the grain boundaries hold.
        Tool dc = stock(bd);
        for (int i = 0; i < 6; ++i) draw(dc, 0.5, COLD);
        check(soundness(dc) == 1.0,
              "cold, the poison sleeps: below the eutectic even a sulfurous bar holds together");

        // (e) the trap. The dirty bar cannot be worked hot (it cracks) and the cold
        // end where it is sound is exactly where it hardens toward brittleness. The
        // clean bar has no such bind. Sulfur turns the whole heat-axis against you.
        check(soundness(dh) < 0.5 && soundness(dc) == 1.0 && hardness(dc) > hardness(dh) + 50.0,
              "sulfurous iron is trapped: it red-shorts hot and only hardens cold, sound at neither working heat");

        // (f) cracking is monotone in hot work: every hot blow on a sulfurous bar
        // spends more soundness, without bound toward zero.
        Tool one = stock(bd), five = stock(bd);
        draw(one, 0.5, FORGE);
        for (int i = 0; i < 5; ++i) draw(five, 0.5, FORGE);
        check(soundness(one) < 1.0 && soundness(five) < soundness(one),
              "each hot blow costs soundness: red-short worsens monotonically under the hammer");

        const double s_frac = 100.0 * bd.sulfur / (bd.iron + bd.sulfur);
        std::printf("        (bar %.2f%% S: 6 hot blows -> %.0f%% sound (cracked); "
                    "6 cold blows -> %.0f%% sound, %.0f HB (hard))\n",
                    s_frac, 100.0 * soundness(dh), 100.0 * soundness(dc), hardness(dc));
    }

    // ---- 16. Copper: the second metal, and the fork at the furnace -----------
    // Copper shares the whole front of the chain with iron and parts at the
    // furnace. One fire's heat sits above copper's melting point and below iron's,
    // so copper pours LIQUID (cast, and clean -- slag floats off a melt) while iron
    // stays a SOLID sponge (forged, and never clean). Then the two rejoin at the
    // anvil: a cast billet is drawn and hardened by the very same law, on copper's
    // own lower recrystallization floor. The furnace decides everything, and it
    // decides it with two melting points nobody chose.
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };
        auto cu_ore = [](double cup, double qz) {
            Substance o; o.freegrain[CUPRITE][SAND] = cup;
            if (qz > 0.0) o.freegrain[QUARTZ][SAND] = qz;
            return o;
        };
        auto fe_ore = [](double mag, double qz) {
            Substance o; o.freegrain[MAGNETITE][SAND] = mag; o.freegrain[QUARTZ][SAND] = qz; return o;
        };

        const double FURNACE = 1500.0; // a charcoal hearth: above Cu's mp, below Fe's

        // (a) the straddle itself: the same heat that melts copper leaves iron solid.
        check(FURNACE > TM_COPPER && FURNACE < TM_IRON
              && smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), FURNACE).molten,
              "the furnace straddles the two melting points: one fire pours copper liquid and would leave iron solid");

        // (b) the copper ledger balances, element by element, like the bloomery's.
        const MeltResult m = smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), FURNACE);
        double in_ore[N_ELEM], in_fuel[N_ELEM], in_cu[N_ELEM];
        assay_elements(cu_ore(2.0, 1.0), in_ore);
        assay_elements(charcoal(1.0), in_fuel);
        for (int e = 0; e < N_ELEM; ++e) in_cu[e] = in_ore[e] + in_fuel[e];
        bool cu_balanced = m.metal_cu > 0.0;
        for (int e = 0; e < N_ELEM; ++e) {
            const double metal_e = (e == EL_CU) ? m.metal_cu : 0.0;
            cu_balanced = cu_balanced && std::fabs(in_cu[e] - (metal_e + m.slag[e] + m.gas[e])) < 1e-12;
        }
        check(cu_balanced, "the copper ledger balances: charge = metal + slag + gas, element by element");

        // (c) cast clean: a liquid pour sheds its slag by density, so the billet
        // escapes the connectivity floor that leaves a wrought iron bar full of
        // stringers. Same dross in the charge, opposite outcome -- set by state.
        const Billet cu = cast(smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), FURNACE));
        const Bar febar = consolidate(pull_bloom(bloomery(fe_ore(3.0, 1.0), charcoal(2.0))), 8);
        check(cu.cast_clean && billet_slag_fraction(cu) < 1e-9 && slag_fraction(febar) > 0.005,
              "cast copper is clean, wrought iron is not: a liquid sheds its slag, a solid sponge traps it");

        // (d) no wall of the iron kind: silica cannot eat copper. Heap silica onto a
        // copper charge and the metal yield does not move -- there is no cupric
        // fayalite. (The same silica would drag iron into the slag 1.859:1.)
        const double bare  = smelt_copper(cu_ore(2.0, 0.0), charcoal(1.0), FURNACE).metal_cu;
        const double silty = smelt_copper(cu_ore(2.0, 5.0), charcoal(1.0), FURNACE).metal_cu;
        check(bare > 0.0 && std::fabs(bare - silty) < 1e-12,
              "copper has no fayalite wall: silica cannot eat the metal, so a copper charge has no Fe/SiO2 gate");

        // (e) state decides cleanliness, not just cast-vs-forge: a furnace hot enough
        // to REDUCE copper (>800 C) but not to MELT it freezes a slag-trapping
        // sponge -- the same metal, dirty, because it never poured.
        const MeltResult warm = smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), 1200.0);
        const Billet frozen = cast(warm);
        check(warm.metal_cu > 0.0 && !warm.molten && !frozen.cast_clean
              && billet_slag_fraction(frozen) > 0.0,
              "state decides cleanliness: copper that reduces but never melts freezes a dirty sponge, not a clean cast");

        // (f) the rejoin, with teeth: worked at a heat BETWEEN the two metals' recryst
        // floors (copper ~543 K, iron ~724 K), the same draw() hardens iron and
        // anneals copper -- one law, each metal's own floor from its own melting point.
        const double MID = 650.0;
        Tool fe = stock(febar);
        Tool cu2 = stock(cast(smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), FURNACE)));
        for (int i = 0; i < 4; ++i) { draw(fe, 0.5, MID); draw(cu2, 0.5, MID); }
        check(hardness(fe) > H_ANNEALED + 20.0 && std::fabs(hardness(cu2) - H_ANNEALED) < 1e-9,
              "same law, each metal's own floor: at a heat between them iron cold-works and hardens, copper anneals");

        // ...and copper does harden when worked below ITS floor -- it rejoins the
        // curve, it just enters it at a lower temperature.
        Tool cu3 = stock(cast(smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), FURNACE)));
        for (int i = 0; i < 4; ++i) draw(cu3, 0.5, 300.0);
        check(hardness(cu3) > H_ANNEALED + 20.0,
              "copper rejoins the hardening law: worked below its own recryst floor it work-hardens like any metal");

        std::printf("        (furnace %.0f K: Cu pours liquid, Fe would not; cast billet %.2f%% slag vs wrought %.2f%%; "
                    "at %.0f K Fe->%.0f HB, Cu stays %.0f HB)\n",
                    FURNACE, 100.0 * billet_slag_fraction(cu), 100.0 * slag_fraction(febar),
                    MID, hardness(fe), hardness(cu2));
    }

    // ---- 17. Bronze: the alloy, and why a mixture is a new metal -------------
    // Tin is the third metal: trivial to smelt (it melts below a cooking fire, so
    // any reducing heat pours it liquid) but historically the scarce, traded half of
    // the age -- its gate is geology, not the furnace. Alloyed a tenth into copper it
    // makes bronze, and bronze lands OFF THE END of both parents: harder than either
    // AND melting below either. One cause -- the misfit of the tin atom in the copper
    // lattice -- both hardens the solid and loosens the freeze. The alloy is then
    // cast, stocked, drawn and hardened by the very same functions as pure copper:
    // alloying moves the numbers the law reads, never the law.
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };
        auto cu_ore = [](double cup, double qz) {
            Substance o; o.freegrain[CUPRITE][SAND] = cup;
            if (qz > 0.0) o.freegrain[QUARTZ][SAND] = qz;
            return o;
        };
        auto sn_ore = [](double cass, double qz) {
            Substance o; o.freegrain[CASSITERITE][SAND] = cass;
            if (qz > 0.0) o.freegrain[QUARTZ][SAND] = qz;
            return o;
        };
        auto fe_ore = [](double mag, double qz) {
            Substance o; o.freegrain[MAGNETITE][SAND] = mag; o.freegrain[QUARTZ][SAND] = qz; return o;
        };

        const double FURNACE = 1500.0;

        // (a) tin is never a sponge. Its melting point is below copper's and far
        // below iron's, and it sits hundreds of kelvin below any reducing heat, so a
        // fire cool enough to leave copper solid (1200 K) still pours tin liquid.
        const MeltResult sn_hot  = smelt_tin(sn_ore(0.25, 1.0), charcoal(1.0), FURNACE);
        const MeltResult sn_cool = smelt_tin(sn_ore(0.25, 1.0), charcoal(1.0), 1200.0);
        check(TM_TIN < TM_COPPER && TM_COPPER < TM_IRON
              && sn_hot.molten && sn_hot.metal_sn > 0.0
              && sn_cool.molten && !smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), 1200.0).molten,
              "tin is never a sponge: it melts below a cooking fire, so a heat that leaves copper solid still pours tin liquid");

        // (b) the tin ledger balances, element by element, like copper's and iron's.
        double in_ore[N_ELEM], in_fuel[N_ELEM], in_sn[N_ELEM];
        assay_elements(sn_ore(0.25, 1.0), in_ore);
        assay_elements(charcoal(1.0), in_fuel);
        for (int e = 0; e < N_ELEM; ++e) in_sn[e] = in_ore[e] + in_fuel[e];
        bool sn_balanced = sn_hot.metal_sn > 0.0;
        for (int e = 0; e < N_ELEM; ++e) {
            const double metal_e = (e == EL_SN) ? sn_hot.metal_sn : 0.0;
            sn_balanced = sn_balanced && std::fabs(in_sn[e] - (metal_e + sn_hot.slag[e] + sn_hot.gas[e])) < 1e-12;
        }
        check(sn_balanced, "the tin ledger balances: charge = metal + slag + gas, element by element");

        // (c) alloying conserves both metals and sets the tin fraction by mass; both
        // parents cast clean, so the bronze billet carries no slag.
        const MeltResult cup = smelt_copper(cu_ore(2.0, 1.0), charcoal(1.0), FURNACE);
        const Billet     bronze = alloy(cup, sn_hot);
        check(std::fabs(bronze.metal - (cup.metal_cu + sn_hot.metal_sn)) < 1e-12
              && std::fabs(bronze.tin_fraction - sn_hot.metal_sn / (cup.metal_cu + sn_hot.metal_sn)) < 1e-12
              && billet_slag_fraction(bronze) < 1e-9,
              "the alloy conserves both metals: bronze mass = Cu + Sn, tin fraction by mass, and it inherits its parents' cleanliness");

        // (d) alloying is not averaging. A cast bronze -- no cold work whatever -- is
        // harder than cast copper. Two soft metals made a harder one, in the solid,
        // for free: solid-solution strengthening, before a single blow.
        Tool bt = stock(bronze);          // as-cast bronze
        Tool ct = stock(cast(cup));       // as-cast pure copper
        check(bronze.tin_fraction > 0.05 && hardness(bt) > hardness(ct) + 10.0
              && std::fabs(hardness(ct) - H_ANNEALED) < 1e-9,
              "alloying is not averaging: cast bronze is harder than cast copper with no cold work at all");

        // (e) the other gift of the same solute: it lowers the melting point. Bronze
        // pours cooler than copper (and both still below iron). Harder solid and
        // looser freeze are one fact -- the misfit of the tin atom -- read twice.
        check(bronze.tm < TM_COPPER && TM_COPPER < TM_IRON && hardness(bt) > hardness(ct),
              "one solute, both gifts: the tin that hardens the solid also depresses the freeze -- bronze is harder AND melts cooler than copper");

        // (f) same hardening law, a lifted ceiling. Cold-worked to saturation by the
        // identical draw(), bronze tops cold-worked copper -- the curve is the same,
        // the alloy just entered it raised. This is the rejoin, alloyed.
        Tool bw = stock(bronze), cw = stock(cast(cup));
        for (int i = 0; i < 12; ++i) { draw(bw, 0.5, 300.0); draw(cw, 0.5, 300.0); }
        check(hardness(bw) > hardness(cw) + 10.0 && hardness(bw) > H_SATURATED,
              "same law, lifted ceiling: cold-worked bronze tops cold-worked copper, worked by the identical draw()");

        // (g) and it tops cold-worked wrought iron too. On this shared hardness scale
        // fully cold-worked bronze out-hardens fully cold-worked bloomery iron -- the
        // reason the coming of iron did not at once retire bronze: soft wrought iron
        // was no harder, and iron only won once it could be made into steel.
        Tool few = stock(consolidate(pull_bloom(bloomery(fe_ore(3.0, 1.0), charcoal(2.0))), 8));
        for (int i = 0; i < 12; ++i) draw(few, 0.5, 300.0);
        check(hardness(bw) > hardness(few),
              "bronze out-hardens even cold-worked wrought iron: why early iron did not displace it until steel");

        std::printf("        (tin melts at %.0f K, always a pour; bronze %.1f%% Sn melts %.0f K vs Cu %.0f K; "
                    "cast %.0f HB vs Cu %.0f HB; cold-worked %.0f HB vs Fe %.0f HB)\n",
                    TM_TIN, 100.0 * bronze.tin_fraction, bronze.tm, TM_COPPER,
                    hardness(bt), hardness(ct), hardness(bw), hardness(few));
    }

    // ---- 18. Sulfide copper: the roast, and the fire that runs backwards -----
    // The oxide ores are the weathered surface; below them copper is a SULFIDE, and
    // a reducing fire cannot smelt it -- it strips oxygen, and a sulfide has none to
    // give. You must first ROAST it in air (an oxidizing fire, the opposite of the
    // furnace) to burn the sulfur off as SO2 and leave a copper oxide, which then
    // smelts as if it were cuprite. Two fires, opposite atmospheres, fixed order.
    // And it is sulfur's second verdict: invisible-and-incurable in iron (red-short,
    // caught at the anvil), loud-and-curable in copper (caught at the furnace).
    {
        auto charcoal = [](double kg) { Substance c; c.freegrain[CARBON][SAND] = kg; return c; };
        auto cu_ore = [](double cup, double qz) {
            Substance o; o.freegrain[CUPRITE][SAND] = cup;
            if (qz > 0.0) o.freegrain[QUARTZ][SAND] = qz;
            return o;
        };
        auto sulfide_ore = [](double cc, double qz) {
            Substance o; o.freegrain[CHALCOCITE][SAND] = cc;
            if (qz > 0.0) o.freegrain[QUARTZ][SAND] = qz;
            return o;
        };
        auto pyritic_fe = [](double mag, double py, double qz) {
            Substance o; o.freegrain[MAGNETITE][SAND] = mag;
            o.freegrain[PYRITE][SAND] = py; o.freegrain[QUARTZ][SAND] = qz; return o;
        };

        const double FURNACE = 1500.0; // above Cu's mp; a reducing charcoal hearth

        // (a) the reducing furnace has no move on a sulfide. Fed straight to
        // smelt_copper, chalcocite yields NO metal -- there is no oxide to reduce --
        // and its sulfur and copper both report to the slag, untouched.
        const MeltResult direct = smelt_copper(sulfide_ore(2.0, 1.0), charcoal(1.0), FURNACE);
        double in_s[N_ELEM];
        assay_elements(sulfide_ore(2.0, 1.0), in_s);
        check(direct.metal_cu < 1e-12
              && std::fabs(direct.slag[EL_CU] - in_s[EL_CU]) < 1e-12
              && std::fabs(direct.slag[EL_S]  - in_s[EL_S])  < 1e-12,
              "a reducing fire cannot smelt a sulfide: chalcocite gives no copper -- there is no oxygen to strip");

        // (b) the roast conserves copper and drives ALL the sulfur off, element by
        // element. Copper crosses into the calcine as oxide; sulfur leaves as gas;
        // the silica and everything else pass through. (The calcine's new oxygen is
        // atmospheric, so oxygen alone is not conserved -- that is the roast working.)
        const RoastResult rr = roast(sulfide_ore(2.0, 1.0), FURNACE);
        double in_ore[N_ELEM], cal[N_ELEM];
        assay_elements(sulfide_ore(2.0, 1.0), in_ore);
        assay_elements(rr.calcine, cal);
        check(rr.lit
              && std::fabs(cal[EL_CU] - in_ore[EL_CU]) < 1e-12       // copper conserved
              && cal[EL_S] < 1e-12                                   // sulfur gone from the solid
              && std::fabs(rr.gas[EL_S] - in_ore[EL_S]) < 1e-12      // ...and up the stack
              && std::fabs(cal[EL_SI] - in_ore[EL_SI]) < 1e-12       // gangue passed through
              && cal[EL_O] > in_ore[EL_O] + 1e-9,                    // oxygen ADDED from air
              "the roast desulfurizes: copper conserved, all sulfur off as gas, oxygen taken from the air");

        // (c) roast, THEN reduce: the calcine smelts to clean copper, the same metal
        // an equal-copper oxide ore would give. The sulfide is won -- but only by the
        // two fires in sequence.
        const MeltResult from_sulfide = smelt_copper(rr.calcine, charcoal(1.0), FURNACE);
        const double cu_equiv = in_ore[EL_CU];
        const MeltResult from_oxide = smelt_copper(cu_ore(cu_equiv / element_fraction(CUPRITE, EL_CU), 1.0),
                                                   charcoal(1.0), FURNACE);
        const Billet cast_sulfide = cast(from_sulfide);
        check(from_sulfide.metal_cu > 0.0
              && std::fabs(from_sulfide.metal_cu - from_oxide.metal_cu) < 1e-9
              && cast_sulfide.cast_clean && billet_slag_fraction(cast_sulfide) < 1e-9,
              "roast then reduce wins the metal: the calcine pours the same clean copper an oxide ore would");

        // (d) it takes BOTH fires, in order. The reducing fire alone gives nothing
        // (a); and a fire too cool to roast (below ROAST_T) converts no sulfide, so
        // the reducing fire that follows still finds no oxide and pours no metal.
        const RoastResult cold = roast(sulfide_ore(2.0, 1.0), 900.0); // < ROAST_T
        const MeltResult after_cold = smelt_copper(cold.calcine, charcoal(1.0), FURNACE);
        check(!cold.lit && direct.metal_cu < 1e-12 && after_cold.metal_cu < 1e-12,
              "two fires, fixed order: neither the reduce alone nor a failed roast gives copper -- only oxidize-then-reduce");

        // (e) sulfur's two verdicts, side by side. The copper's sulfur left at the
        // FURNACE (the roast), so no sulfur reaches the cast metal at all. The iron's
        // sulfur is invisible at the furnace and rides into the bloom, seeding the
        // red-short the anvil will find. Same element; cured in one metal, hidden in
        // the other -- because copper's sulfide roasts and iron's contaminant does not.
        const BloomResult pybloom = bloomery(pyritic_fe(3.0, 0.5, 1.0), charcoal(2.0));
        check(cal[EL_S] < 1e-12 && from_sulfide.slag[EL_S] < 1e-12   // copper: no sulfur, furnace-cured
              && pybloom.bloom_sulfur > 0.0,                          // iron: sulfur into the metal, anvil-bound
              "one element, two verdicts: copper's sulfur is roasted off at the furnace; iron's rides silently to the anvil");

        std::printf("        (chalcocite %.0f%% Cu, %.0f%% S: reduce-only yields 0 kg Cu; roast drives off %.3f kg S, "
                    "then reduce pours %.3f kg clean Cu; pyritic bloom keeps %.3f kg S for the anvil)\n",
                    100.0 * element_fraction(CHALCOCITE, EL_CU), 100.0 * element_fraction(CHALCOCITE, EL_S),
                    rr.gas[EL_S], from_sulfide.metal_cu, pybloom.bloom_sulfur);
    }

    // ---- 19. Geography: the ground as a field, and the co-location gate -----
    // The first spatial model. Composition = f(x, y, tier): point at the ground,
    // scoop, read the panel; move, and the makeup changes. Most of the valley is
    // barren. An ore body is graded (rises toward a center), not a node. Depth
    // changes the MINERAL -- oxide cap over sulfide root -- so a full-depth dig
    // heaps a mixed column that must be cobbed. And copper and tin sit far apart,
    // so no spot yields both: bronze forces travel. None of these assert a layout
    // number; they assert the SHAPE of the field (issue #28).
    {
        const Place cu_center{0.0, 0.0};
        const Place tin_center{300.0, 120.0};
        const Place nowhere{150.0, -200.0}; // far from every body

        // (a) most ground is barren: a scoop far from any body is gangue, and the
        // panel reads nothing. This is why sampling is an activity at all.
        const Substance waste = sample(nowhere, SURFACE, 1.0);
        check(waste.grade(QUARTZ) > 0.999
              && waste.grade(CUPRITE) < 1e-9 && waste.grade(CASSITERITE) < 1e-9,
              "most ground is barren: a scoop far from any body is all gangue, the panel reads nothing");

        // (b) an ore body is graded, not a node: copper richness climbs smoothly
        // toward the center, with no step and no plug-in spot. Walk in from the rim.
        double last = -1.0;
        bool monotone = true;
        for (double r : {40.0, 30.0, 20.0, 10.0, 0.0}) {
            const double g = sample(Place{r, 0.0}, SURFACE, 1.0).grade(CUPRITE);
            if (g < last - 1e-12) monotone = false;
            last = g;
        }
        const double g_center = sample(cu_center, SURFACE, 1.0).grade(CUPRITE);
        const double g_edge = sample(Place{38.0, 0.0}, SURFACE, 1.0).grade(CUPRITE);
        check(monotone && g_edge > 0.0 && g_center > g_edge,
              "an ore body is graded, not a node: copper richness climbs smoothly toward the center");

        // (c) depth changes the MINERAL, not just the amount: an oxide cap the fire
        // smelts straight, a sulfide root the fire refuses until roasted. The roast
        // (Section 18) lives underground.
        const Substance cap = sample(cu_center, SURFACE, 1.0);
        const Substance deep = sample(cu_center, DEEP, 1.0);
        check(cap.grade(CUPRITE) > 0.0 && cap.grade(CHALCOCITE) < 1e-12
              && deep.grade(CHALCOCITE) > 0.0 && deep.grade(CUPRITE) < 1e-12,
              "depth changes the mineral: oxide cap on top, sulfide root below -- the gossan worked before the deep ore");

        // (d) the mixed column: a full-depth dig heaps oxide AND sulfide AND waste
        // into one pile -- richer haul, harder problem -- and mass is conserved
        // across the tiers. No single tier holds both minerals; the pile does,
        // which is why parting it is a job downstream, not at the hole.
        const Substance pile = dig_column(cu_center, 1.0);
        check(pile.grade(CUPRITE) > 0.0 && pile.grade(CHALCOCITE) > 0.0
              && pile.grade(QUARTZ) > 0.0
              && std::fabs(pile.total_mass() - N_TIER * 1.0) < 1e-12,
              "the mixed column: a full-depth dig heaps oxide, sulfide and waste in one pile that must be sorted");

        // (e) co-location is the gate: the copper body carries no tin and the tin
        // body no copper, and they are hundreds of meters apart. Bronze cannot be
        // dug from one hole -- it forces you to travel.
        const Substance at_sn = dig_column(tin_center, 1.0);
        check(pile.grade(CASSITERITE) < 1e-12                              // copper body: no tin
              && at_sn.grade(CASSITERITE) > 0.0
              && at_sn.grade(CUPRITE) < 1e-12 && at_sn.grade(CHALCOCITE) < 1e-12, // tin body: no copper
              "co-location is the gate: copper carries no tin and tin no copper -- bronze forces travel");

        // (f) hands sample, the pick wins: the tool sets the mass, never the
        // makeup. Same grade on the panel, ten times the load. (The shovel is not
        // the winning tool -- it is the loader downstream in the carry loop.)
        const Substance hand = sample(cu_center, SURFACE, 0.1);
        const Substance pick = sample(cu_center, SURFACE, 1.0);
        check(std::fabs(pick.total_mass() - 10.0 * hand.total_mass()) < 1e-12
              && std::fabs(pick.grade(CUPRITE) - hand.grade(CUPRITE)) < 1e-12,
              "hands sample, the pick wins: the tool sets the load, not the makeup");

        // (g) liberation is a SIZE, not a toll, and ORIGIN sets it. The tin creek is
        // a placer: the river weathered the cassiterite free and sorted it, so it
        // comes up essentially all FREE at the face -- pannable on the spot, the
        // crusher never needed. The disseminated sulfide root is hard rock ground to
        // microns, so it comes up essentially all LOCKED, freeing almost nothing
        // until the breaker crushes it toward grain size. This is why placers were
        // panned by one hand and hard rock built stamp mills; it is also the fact the
        // carry loop's cost turns on.
        auto free_fraction = [](const Substance& sub, int mineral) {
            double free_m = 0.0, tot_m = 0.0;
            for (int s = 0; s < N_SIZE; ++s) {
                free_m += sub.freegrain[mineral][s];
                tot_m  += sub.freegrain[mineral][s]
                          + COMPOSITE_TARGET_FRACTION * sub.composite[mineral][s];
            }
            return tot_m > 0.0 ? free_m / tot_m : 0.0;
        };
        const double tin_free  = free_fraction(dig_column(tin_center, 1.0), CASSITERITE);
        const Substance root   = sample(cu_center, DEEP, 1.0);       // chalcocite, disseminated
        const double root_free = free_fraction(root, CHALCOCITE);
        const double root_crushed = free_fraction(crush(root, 1.0), CHALCOCITE);
        check(tin_free > 0.99            // placer tin: all but free at the face
              && root_free < 0.01        // sulfide root: all but locked
              && root_crushed > root_free, // and the breaker is what frees it
              "liberation is a size, not a toll: placer tin comes up free at the face, the sulfide root stays locked until the breaker");

        // (h) free is not won: origin decides whether a WASH concentrates the ore.
        // The wash the carry slice does -- screen the coarse off (cut at GRAVEL),
        // pan the sand -- must LIFT the grade at the placer and barely move it at the
        // hard-rock hill. This is the whole reason the tin creek is workable with a
        // pan and the copper hill is not, and it is what an earlier "all tin free in
        // the gravel" model got wrong: free coarse tin a low pan cut cannot sort from
        // coarse quartz is free but not won.
        auto washed_grade = [](const Substance& heap, int mineral) {
            const ScreenResult sc = screen(heap, 0.95, GRAVEL);   // cobble off, sand under
            const Substance conc = separate(sc.undersize, PAN).concentrate;
            const double m = conc.total_mass();
            return m > 0.0 ? conc.freegrain[mineral][SAND] / m : 0.0; // free ore grade of the pan product
        };
        const double tin_wash  = washed_grade(dig_column(tin_center, 1.0), CASSITERITE);
        const double tin_feed  = dig_column(tin_center, 1.0).grade(CASSITERITE);
        const Substance cu_pile = dig_column(cu_center, 1.0);
        const double cu_wash   = washed_grade(cu_pile, CUPRITE) + washed_grade(cu_pile, CHALCOCITE);
        check(tin_wash > tin_feed + 0.2      // placer: the wash concentrates the tin
              && cu_wash < 0.10,             // hard rock: the wash wins almost nothing
              "free is not won: a wash concentrates the placer tin (grade climbs, no stone) but barely touches the locked copper");

        const double sep = std::sqrt(300.0 * 300.0 + 120.0 * 120.0);
        const double cap_face = free_fraction(sample(cu_center, SURFACE, 1.0), CUPRITE);
        std::printf("        (copper body: surface %.0f%% cuprite, deep %.0f%% chalcocite; full column "
                    "%.0f%% oxide + %.0f%% sulfide + %.0f%% waste; at the face tin %.0f%% free, "
                    "oxide cap %.0f%% free, sulfide %.1f%% free; tin body %.0f m away, 0%% copper)\n",
                    100.0 * cap.grade(CUPRITE), 100.0 * deep.grade(CHALCOCITE),
                    100.0 * pile.grade(CUPRITE), 100.0 * pile.grade(CHALCOCITE),
                    100.0 * pile.grade(QUARTZ),
                    100.0 * tin_free, 100.0 * cap_face, 100.0 * root_free, sep);
        std::printf("        (a wash -- screen off the cobble, pan the sand -- lifts the placer "
                    "tin %.0f%% -> %.0f%% grade with no stone; the same wash at the copper hill "
                    "reaches %.0f%%)\n",
                    100.0 * tin_feed, 100.0 * tin_wash, 100.0 * cu_wash);
    }

    // ---- Fuel: the two gates a fire has -----------------------------------
    // Charcoal is the reductant (a chemical gate); peak temperature is the melt
    // gate (thermal); the two are independent -- fuel.h. A wood fire clears the
    // reduction onset and still smelts nothing, because wood carries no carbon.
    {
        const double wood = 2.0;
        const Substance charcoal = pyrolyze(wood, 0.0);
        check(std::fabs(charcoal.phase_mass(CARBON) - wood * CHAR_YIELD) < 1e-12,
              "pyrolysis: charcoal carbon is CHAR_YIELD of the dry wood, landing in Substance as CARBON");

        // Gate 2, the melt tiers, reproduce history against smelt.h's own numbers.
        check(peak_temperature(WOOD, OPEN) > TM_TIN
              && peak_temperature(WOOD, OPEN) < TM_COPPER,
              "tiers: an open wood fire pours tin but cannot pour copper");
        check(peak_temperature(CHARCOAL, NATURAL) >= TM_COPPER
              && peak_temperature(CHARCOAL, NATURAL) < BLOOM_ENVELOPE_T,
              "tiers: charcoal on natural draft pours copper but cannot reach the bloom");
        check(peak_temperature(CHARCOAL, FORCED) >= BLOOM_ENVELOPE_T
              && peak_temperature(CHARCOAL, FORCED) < TM_IRON,
              "tiers: only charcoal under forced air reaches the bloom -- and even it never melts iron");

        // Gate 1, the reductant, is chemical not thermal. A clean hematite charge
        // (no silica, so the fayalite wall is not what is under test) reduces
        // NOTHING on a carbon-less fire, however reducing the envelope is called --
        // wood carries no carbon into the charge. The same charge smelts on charcoal.
        Substance ore; ore.freegrain[HEMATITE][SAND] = 1.0;
        const Substance no_fuel;   // a wood fire: heat, but zero carbon in the charge
        const BloomResult dry_fire  = bloomery(ore, no_fuel,        /*reducing=*/true);
        const BloomResult with_char = bloomery(ore, pyrolyze(wood), /*reducing=*/true);
        check(dry_fire.bloom_iron == 0.0 && with_char.bloom_iron > 0.0,
              "two gates: a hot carbon-less fire reduces no iron -- the reductant, not the heat, is the gate");

        std::printf("\n  fire tiers (K) vs smelt gates: tin %.0f, copper %.0f, bloom %.0f, iron-melt %.0f\n",
                    TM_TIN, TM_COPPER, BLOOM_ENVELOPE_T, TM_IRON);
        std::printf("  %-26s %8.0f\n", "wood, open", peak_temperature(WOOD, OPEN));
        std::printf("  %-26s %8.0f\n", "charcoal, natural draft", peak_temperature(CHARCOAL, NATURAL));
        std::printf("  %-26s %8.0f\n", "charcoal, forced air", peak_temperature(CHARCOAL, FORCED));
        std::printf("  char yield %.0f%%: %.2f kg wood -> %.2f kg charcoal -> %.2f kg iron (silica-free charge)\n",
                    100.0 * CHAR_YIELD, wood, charcoal.phase_mass(CARBON), with_char.bloom_iron);
    }

    // ---- The charcoal pit: the seal is the gate ---------------------------
    // The station that makes the reductant, and its one failure mode -- char.h.
    // A sealed pit reproduces the ideal pyrolysis yield; air eats the char before
    // it can be drawn, to nearly nothing wide open. The finding is the ordering,
    // not the retained fractions.
    {
        const double wood = 3.0;
        const Substance sealed = char_pit(wood, 0.0, PIT_SEALED);
        const Substance vented = char_pit(wood, 0.0, PIT_VENTED);
        const Substance open   = char_pit(wood, 0.0, PIT_OPEN);

        // A fully sealed pit loses nothing to the fire: it IS pyrolyze().
        check(std::fabs(sealed.phase_mass(CARBON) - pyrolyze(wood, 0.0).phase_mass(CARBON)) < 1e-12,
              "char pit: a sealed pit yields the full pyrolysis char -- the ideal transform");
        // Air is the gate: more of it, less char, monotonically.
        check(sealed.phase_mass(CARBON) > vented.phase_mass(CARBON)
              && vented.phase_mass(CARBON) > open.phase_mass(CARBON),
              "char pit: the looser the seal, the less char survives -- air burns the product");
        // Wide open it is a bonfire: char is nearly all consumed to ash.
        check(open.phase_mass(CARBON) < 0.2 * sealed.phase_mass(CARBON),
              "char pit: an open pit leaves almost no charcoal -- it is a fire, not a kiln");
        // What survives is still the reductant a smelt reads: free CARBON.
        check(sealed.freegrain[CARBON][GRAVEL] > 0.0,
              "char pit: the drawn char is free CARBON -- exactly what smelt.h reduces with");
    }

    // ---- Knapping: fracture is the gate, and the edge has a floor ----------
    // The first tool the hands can make -- stone to edge, knap.h. The stone you
    // pick up is the wrong stone (gather's loose cobble is a hammerstone); an edge
    // wants a conchoidal core, and even a clean flake is only as keen as the stone
    // floor, a wall the next material -- not effort -- gets under.
    {
        const Stone core   { 1.0, CONCHOIDAL };   // a flint nodule: breaks the right way
        const Stone cobble { 1.0, GRANULAR   };   // gather's loose stone: a hammerstone

        // Gate 1, material: the conchoidal core struck with the granular hammer gives
        // an edge; every wrong pairing gives only grit. Fracture, not hardness.
        const KnapResult good = knap(core,   cobble, STRIKE_ACUTE);
        const KnapResult soft = knap(cobble, cobble, STRIKE_ACUTE);   // granular core: powders
        const KnapResult self = knap(core,   core,   STRIKE_ACUTE);   // brittle hammer: shatters
        check(good.flake.usable && !soft.flake.usable && !self.flake.usable,
              "knap: only a conchoidal core under a tough hammer takes an edge -- fracture is the gate");

        // Gate 2, the blow: the same right stones give an edge, a dud, or a ruin.
        const KnapResult steep = knap(core, cobble, STRIKE_STEEP);
        const KnapResult hard  = knap(core, cobble, STRIKE_HARD);
        check(!steep.flake.usable && steep.debris == 0.0 && steep.flake.mass > 0.0,
              "knap: a steep blow hinges a flake off short -- mass leaves, no edge");
        check(!hard.flake.usable && hard.debris > 0.0 && hard.flake.mass == 0.0,
              "knap: an overpowered blow shatters the core -- mass to debris, no flake");

        // The staircase floor: a stone edge is only ever so keen. The number is a
        // stand-in; that it is a floor no strike beats is the finding.
        check(good.flake.usable && good.flake.edge_angle == STONE_EDGE_FLOOR,
              "knap: a clean stone edge lands at its floor angle -- the wall metal, not effort, gets under");

        // Authors nothing: every blow only conserves mass -- flake + core + debris
        // equals the stone you struck, on every outcome.
        auto conserves = [](const Stone& c0, const KnapResult& r) {
            return std::fabs((r.flake.mass + r.core.mass + r.debris) - c0.mass) < 1e-12;
        };
        check(conserves(core, good) && conserves(core, steep) && conserves(core, hard)
              && conserves(cobble, soft),
              "knap: the edge is subtracted, never added -- flake + core + debris conserves");
    }

    // ---- Hafting: the haft is force, and the joint is the ceiling ----------
    // The rung above knap -- edge + stick -> the first tool, haft.h. Hafting does
    // not sharpen the edge (knap floored that); it puts the edge on a lever. Two
    // findings: the haft multiplies force (a hafted axe fells what the bare hand
    // and bare edge cannot), and the joint is the weakest link (a failed coupling
    // delivers nothing however keen the head or long the haft).
    {
        const StoneEdge head { 0.40, STONE_EDGE_FLOOR, true };   // a worked stone bit, knap's floored edge
        const StoneEdge dud  { 0.40, 0.0,              false };  // a hinged flake -- no edge

        // The bootstrap tools: the crude first axe (sapling, lashed) and the axe after
        // (timber, seated), plus the same head in the bare fist.
        const Hafted first  = haft(head, 0.55, SAPLING, BIND_LASHED, EDGE_ACROSS);
        const Hafted proper = haft(head, 0.70, TIMBER,  BIND_SEATED, EDGE_ACROSS);

        // Finding 1, the lever: the haft, not the edge, is what fells timber. The bare
        // hand cannot clear the fell floor (gather's wall); even the crude sapling axe
        // can, and the timber axe far more -- and the edge never got keener to do it.
        check(hand_bite(head) < FELL_ENERGY,
              "haft: a bare edge in the fist cannot fell -- the hand is a lever of no length (gather's wall)");
        check(first.fells() && first.bite() > hand_bite(head),
              "haft: the crude first axe -- a floored edge on a sapling -- fells what the bare hand could not");
        check(proper.bite() > first.bite(),
              "haft: force scales with the lever -- a longer, stiffer timber haft hits far harder");

        // Finding 2, the joint is the ceiling: a keen head on a long haft delivers
        // NOTHING through a coupling that lets it walk off. The tool is its worst link.
        const Hafted loose = haft(head, 0.70, TIMBER, BIND_NONE, EDGE_ACROSS);
        check(!loose.sound && loose.bite() == 0.0,
              "haft: no joint, no tool -- a perfect head on a long haft delivers nothing if the head walks off");
        // The weakest link, precisely: with the best bind the STOCK becomes the ceiling,
        // so the sapling axe is worse than the timber axe because the WOOD changed, not
        // the binding -- which is exactly why the first axe must wait on the first fell.
        const Hafted seat_sap = haft(head, 0.70, SAPLING, BIND_SEATED, EDGE_ACROSS);
        const Hafted seat_tim = haft(head, 0.70, TIMBER,  BIND_SEATED, EDGE_ACROSS);
        check(seat_sap.joint < seat_tim.joint && seat_sap.joint == joint_strength(BIND_SEATED, SAPLING),
              "haft: with the bind maxed the haft stock is the ceiling -- the sapling axe waits on timber");

        // The staircase carries knap's floor untouched: hafting adds force, not keenness.
        check(first.edge_angle == STONE_EDGE_FLOOR && proper.edge_angle == STONE_EDGE_FLOOR,
              "haft: the edge stays at knap's floor -- hafting multiplies force, never sharpness");

        // knap's gate carried forward: a dud flake hafts into a club, not a tool.
        const Hafted club = haft(dud, 0.70, TIMBER, BIND_SEATED, EDGE_ACROSS);
        check(!club.sound,
              "haft: no usable edge, no tool -- a hinged flake on a fine haft is only a club");

        // Head geometry names the tool -- same mechanics, different work.
        check(haft(head, 0.7, TIMBER, BIND_SEATED, EDGE_ACROSS).kind == AXE
              && haft(head, 0.7, TIMBER, BIND_SEATED, HEAD_POINT).kind == PICK
              && haft(head, 0.7, TIMBER, BIND_SEATED, EDGE_INLINE).kind == ADZE,
              "haft: the head's geometry decides the tool -- axe fells, pick wins rock, adze dresses wood");
    }

    // ---- The picture -------------------------------------------------------
    std::printf("\n  settling velocity, m/s (free grains)\n  %-10s %10s %10s %10s %10s\n",
                "phase", sz(0), sz(1), sz(2), sz(3));
    for (int p : {CARBON, QUARTZ, GOETHITE, MAGNETITE, HEMATITE}) {
        std::printf("  %-10s", PHASES[p].id);
        for (int s = 0; s < N_SIZE; ++s) std::printf(" %10.3e", free_velocity(p, s));
        std::printf("\n");
    }

    std::printf("\n  grade/recovery, magnetite from sized river sand (feed grade %.3f)\n", feed.grade(MAGNETITE));
    std::printf("  %-8s %6s %10s %10s %8s\n", "tool", "9^sig", "cut m/s", "recovery", "grade");
    for (const SeparatorParams* sp : {&HANDS, &PAN, &SLUICE})
        for (double cut : {0.030, 0.050, 0.070, 0.090}) {
            SeparatorParams q = *sp; q.cut_velocity = cut;
            const SeparationResult r = separate(feed, q);
            std::printf("  %-8s %6.2f %10.3f %10.3f %8.3f\n", sp->name, imperfection(*sp), cut,
                        recovery(feed, r.concentrate, MAGNETITE), r.concentrate.grade(MAGNETITE));
        }

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ok", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
