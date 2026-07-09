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

        bool by_density = true;
        for (int s = 0; s < N_SIZE; ++s)
            for (int p = 0; p < N_PHASE; ++p)
                for (int q = 0; q < N_PHASE; ++q)
                    if (PHASES[p].density > PHASES[q].density && free_velocity(p, s) <= free_velocity(q, s))
                        by_density = false;
        check(by_density, "within one size class, velocity ranks by density: this is what a pan reads");

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

        // Levigation is the one process in the design that quotes settling times,
        // and it is also the one place Stokes is exact. So the times are not a
        // fact about clay. They are a fact about clay and the depth of your hole.
        const double DEPTH = 0.10; // m of standing water. AUTHORED: it's a puddle.
        const double d_sand_fine = 62.5e-6, d_silt = 20.0e-6, d_clay = 2.0e-6;
        const double t_sand = DEPTH / stokes_velocity(PHASES[QUARTZ].density, d_sand_fine);
        const double t_silt = DEPTH / stokes_velocity(PHASES[QUARTZ].density, d_silt);
        const double t_clay = DEPTH / stokes_velocity(PHASES[KAOLINITE].density, d_clay);
        check(reynolds(stokes_velocity(PHASES[QUARTZ].density, d_sand_fine), d_sand_fine) < 1.0,
              "levigation lives entirely in creeping flow, so its settling times are exactly derivable");
        check(t_sand < t_silt && t_silt < t_clay,
              "levigation separates by settling time, and the order is sand, silt, clay");
        std::printf("        (0.10 m of water: sand %.0f s | silt %.0f s | clay %.1f h)\n",
                    t_sand, t_silt, t_clay / 3600.0);
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

        // Volume buys recovery -- and so does depth, which the algebra says it
        // should not, until you remember the clay is falling too.
        const Vessel flat{0.05, 0.20, "flat"}, deep{0.20, 0.10, "deep"};   // equal volume
        const double r_flat = recovery(dirt, decant(dirt, flat, 14400.0).liquor, KAOLINITE);
        const double r_deep = recovery(dirt, decant(dirt, deep, 14400.0).liquor, KAOLINITE);
        check(std::fabs(flat.volume() - deep.volume()) < 1e-9 && r_deep > 3.0 * r_flat,
              "at equal volume a deep vessel beats a shallow one: the clay is settling too");
        std::printf("        (equal %.1f L: deep recovers %.3f, flat %.3f)\n",
                    flat.volume() * 1000, r_deep, r_flat);

        // A second decant cannot separate a single velocity class. It only taxes it.
        Substance stage = decant(dirt, HOLLOW, 3600.0).liquor;
        const double g0 = stage.grade(KAOLINITE), r0 = recovery(dirt, stage, KAOLINITE);
        for (int i = 0; i < 3; ++i) stage = decant(stage, HOLLOW, 3600.0).liquor;
        const double g3 = stage.grade(KAOLINITE), r3 = recovery(dirt, stage, KAOLINITE);
        check(g3 - g0 < 1e-3 && r3 < 0.90 * r0,
              "re-decanting the liquor is a tax: it cannot divide one velocity class");
        std::printf("        (3 more decants: grade %.6f -> %.6f, recovery %.4f -> %.4f)\n",
                    g0, g3, r0, r3);

        // The ratchet. It has exactly one rung.
        const SeparatorParams raw = fire_pan(dirt);
        check(raw.sharpness > HANDS.sharpness,
              "a pot pinched from unlevigated dirt is a WORSE separator than cupped hands");
        std::printf("        (unlevigated pot sigma %.3f, imperfection %.1f; hands %.2f, %.1f)\n",
                    raw.sharpness, imperfection(raw), HANDS.sharpness, imperfection(HANDS));

        Vessel ves = HOLLOW; double sig[3] = {0, 0, 0};
        for (int gen = 0; gen < 3; ++gen) {
            const Substance body = decant(dirt, ves, 14400.0).liquor;
            sig[gen] = fire_pan(body).sharpness;
            ves = throw_pot(body);
        }
        check(sig[0] < 0.51 * raw.sharpness && std::fabs(sig[1] - sig[0]) < 1e-6
                                            && std::fabs(sig[2] - sig[1]) < 1e-6,
              "and then it stops: generation 1's pan is generation 0's pan, to six places");
        std::printf("        (sigma: raw %.4f -> gen0 %.4f -> gen1 %.4f -> gen2 %.4f)\n",
                    raw.sharpness, sig[0], sig[1], sig[2]);

        // And the one rung is one minute tall. Sand is the only thing coarse
        // enough to blur a pan, and sand is the first thing to fall out of
        // standing water. Everything after that is a rounding error.
        const Substance cobbed = screen(dirt, HAND_COB).undersize;
        const double s_minute = fire_pan(decant(cobbed, HOLLOW, 60.0).liquor).sharpness;
        const double s_day    = fire_pan(decant(dirt, HOLLOW, 14400.0).liquor).sharpness;
        check(s_minute - s_day < 1e-3 && fire_pan(cobbed).sharpness > 1.5 * s_minute,
              "one minute of standing water is worth as much to a pan as four hours");
        std::printf("        (stone-picked dirt %.4f -> 60 s %.4f -> 4 h %.4f)\n",
                    fire_pan(cobbed).sharpness, s_minute, s_day);

        // Because the grade ceiling was never a property of the tool.
        double cb = 0.0;
        for (int p = 0; p < N_PHASE; ++p) cb += dirt.freegrain[p][CLAY];
        const double ceiling = dirt.freegrain[KAOLINITE][CLAY] / cb;
        bool pinned = true;
        for (const Vessel& v : {HOLLOW, Vessel{0.30, 0.15, "pot"}, Vessel{0.40, 0.30, "vat"}})
            for (double t : {3600.0, 14400.0}) {
                const Substance liq = decant(dirt, v, t).liquor;
                if (liq.total_mass() > 1e-9 && std::fabs(liq.grade(KAOLINITE) - ceiling) > 0.01) pinned = false;
            }
        check(pinned, "every vessel and every wait lands on the same grade: the dirt's own clay bin");
        std::printf("        (ceiling %.4f; kaolinite/quartz velocity ratio in clay is %.3fx --\n"
                    "         the same 1.03x that makes magnetite indistinguishable from hematite)\n",
                    ceiling, free_velocity(QUARTZ, CLAY) / free_velocity(KAOLINITE, CLAY));

        // Grade pinned + recovery raised = the curve moved OUTWARD. A bigger pot
        // is a better tool by this project's own definition, and it is not
        // throughput: it digs no more dirt, it loses less of the clay in what it
        // has. Throughput would be a bigger shovel, and a bigger shovel moves
        // nothing. That distinction is one the README got wrong for an hour.
        const Vessel pot{0.30, 0.15, "pot"};
        const Substance lh = decant(dirt, HOLLOW, 14400.0).liquor;
        const Substance lp = decant(dirt, pot,    14400.0).liquor;
        const double rec_h = recovery(dirt, lh, KAOLINITE), rec_p = recovery(dirt, lp, KAOLINITE);
        check(std::fabs(lp.grade(KAOLINITE) - lh.grade(KAOLINITE)) < 0.01 && rec_p > 10.0 * rec_h,
              "a bigger pot moves the curve outward: same grade, an order more recovery");

        Substance big = dirt; big.add(dirt);   // a bigger shovel: twice the charge
        const Substance lb = decant(big, HOLLOW, 14400.0).liquor;
        check(recovery(big, lb, KAOLINITE) < rec_h,
              "and a bigger shovel moves nothing outward: that is what throughput is");
        std::printf("        (recovery: hollow %.4f, pot %.4f, hollow with a double charge %.4f)\n",
                    rec_h, rec_p, recovery(big, lb, KAOLINITE));
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
