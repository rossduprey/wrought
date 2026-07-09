// Phase A, step 1: pan a bucket of river sand.
//
// DESIGN.md proposes conservation of mass as the unit test. It is necessary and
// far too weak — a separator that does nothing conserves mass perfectly. Every
// test below asserts a *relationship*, so none of them can be satisfied by
// tuning the fixture, and correcting a density must not change any outcome.
//
// Build: make test

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

#include "separate.h"

using namespace wrought;

static int failures = 0;

static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "  pass" : "* FAIL", what);
    if (!ok) ++failures;
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
        const double m[N_SIZE] = {10.0*frac*0.15, 10.0*frac*0.75, 10.0*frac*0.10};
        const double l[N_SIZE] = {lf, ls, lg};
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
    const double q[N_SIZE] = {0.20, 1.50, 5.30}, ql[N_SIZE] = {1, 1, 1};
    const double f[N_SIZE] = {0.05, 0.30, 1.00}, fl[N_SIZE] = {0.90, 0.60, 0.30};
    const double m[N_SIZE] = {0.02, 0.15, 1.30}, ml[N_SIZE] = {0.85, 0.35, 0.10};
    const double h[N_SIZE] = {0.01, 0.05, 0.12}, hl[N_SIZE] = {0.80, 0.30, 0.10};
    b.put(FELDSPAR, f, fl); b.put(MAGNETITE, m, ml); b.put(HEMATITE, h, hl); b.put(QUARTZ, q, ql);
    return b.build();
}

struct Point { double cut, rec, grade; };

static std::vector<Point> frontier(const Substance& feed, const SeparatorParams& sp, int target) {
    std::vector<Point> pts;
    for (double cut = 2.40; cut <= 5.40; cut += 0.05) {
        SeparatorParams q = sp; q.cut_density = cut;
        const SeparationResult r = separate(feed, q);
        if (r.concentrate.total_mass() <= 1e-9) continue;
        pts.push_back({cut, recovery(feed, r.concentrate, target), r.concentrate.grade(target)});
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
        const double m[N_SIZE] = {10.0*frac*0.15, 10.0*frac*0.75, 10.0*frac*0.10};
        const double l[N_SIZE] = {lf, ls, lg};
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

int main() {
    const Substance feed = river_sand();

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

        const Substance c = crush(feed, 0.6);
        bool crush_ok = true;
        for (int p = 0; p < N_PHASE; ++p)
            if (std::fabs(c.phase_mass(p) - feed.phase_mass(p)) > 1e-9) crush_ok = false;
        check(crush_ok, "crushing conserves every phase (breakage moves mass, it does not make it)");

        const ScreenResult sr = screen(feed, 0.8);
        check(std::fabs(sr.oversize.total_mass() + sr.undersize.total_mass() - feed.total_mass()) < 1e-12,
              "screening conserves mass");
    }

    // ---- 2. Grade trades against recovery ----------------------------------
    {
        const auto f = frontier(feed, PAN, MAGNETITE);
        bool mono = true;
        for (size_t i = 1; i < f.size(); ++i) {
            if (f[i].rec > f[i-1].rec + 1e-9) mono = false;
            if (f[i].grade < f[i-1].grade - 1e-9) mono = false;
        }
        check(mono, "raising the cut strictly trades recovery away for grade, at every step");
        check(f.front().rec > f.back().rec && f.front().grade < f.back().grade,
              "the two ends of the sweep are the two ends of the tradeoff");
        check(f.front().grade > feed.grade(MAGNETITE),
              "even the gentlest pan enriches: a separation is not a no-op");
    }

    // ---- 3. The tool moves the curve ---------------------------------------
    // Isolate sharpness: same size efficiency, only the cut is blurrier. If the
    // central claim is real, the sharper tool wins at EVERY matched recovery,
    // not merely at its best point.
    {
        SeparatorParams dull = PAN, sharp = PAN;
        dull.sharpness = 0.80; sharp.sharpness = 0.35;
        const auto fd = frontier(feed, dull, MAGNETITE);
        const auto fs = frontier(feed, sharp, MAGNETITE);

        bool dominates = true; int compared = 0;
        for (double rec = 0.30; rec <= 0.90; rec += 0.05) {
            double gd, gs;
            if (grade_at_recovery(fd, rec, gd) && grade_at_recovery(fs, rec, gs)) {
                ++compared;
                if (gs <= gd) dominates = false;
            }
        }
        check(compared >= 10, "the two tools overlap on enough of the recovery range to compare");
        check(dominates, "a sharper cut dominates a duller one at EVERY matched recovery");
    }

    // ---- 4. The liberation ceiling is real, and it is f --------------------
    // Every particle of a fully locked phase is half gangue by mass. No cut, no
    // tool, no number of passes can produce a concentrate richer than that.
    {
        const Substance all_locked = with_magnetite_liberation(0.0);
        bool ceiling = true;
        for (const SeparatorParams* sp : {&HANDS, &PAN, &SLUICE})
            for (double cut = 2.4; cut <= 5.4; cut += 0.1) {
                SeparatorParams q = *sp; q.cut_density = cut;
                if (separate(all_locked, q).concentrate.grade(MAGNETITE) > COMPOSITE_TARGET_FRACTION + 1e-9) ceiling = false;
            }
        check(ceiling, "fully locked magnetite can never exceed the composite's own grade");

        const double g03 = peak_grade_over_passes(with_magnetite_liberation(0.3), PAN, 64, MAGNETITE);
        const double g05 = peak_grade_over_passes(with_magnetite_liberation(0.5), PAN, 64, MAGNETITE);
        const double g10 = peak_grade_over_passes(with_magnetite_liberation(1.0), PAN, 64, MAGNETITE);
        check(g03 < g05 && g05 < g10, "liberation orders the best grade any amount of work can reach");
        std::printf("        (best attainable grade: L=0.3 %.3f | L=0.5 %.3f | L=1.0 %.3f)\n", g03, g05, g10);
    }

    // ---- 5. A density separator ranks by density, and nothing else ---------
    // Re-panning forever does not converge on your target. It converges on the
    // densest phase present. Hematite is denser than magnetite, so in the limit
    // the pan throws your magnetite away. This is why the lodestone exists, and
    // it is not a cheat: it reads an axis density cannot see.
    {
        Substance cur = feed;
        double best = 0.0; int best_n = 0;
        for (int n = 1; n <= 256; ++n) {
            cur = separate(cur, PAN).concentrate;
            if (cur.grade(MAGNETITE) > best) { best = cur.grade(MAGNETITE); best_n = n; }
        }
        check(best_n > 1 && best_n < 256, "magnetite grade peaks at a finite number of passes, then falls");
        check(cur.grade(HEMATITE) > cur.grade(MAGNETITE),
              "in the limit the concentrate is the densest phase, not the wanted one");
        check(recovery(feed, cur, MAGNETITE) < 0.05 * recovery(feed, separate(feed, PAN).concentrate, MAGNETITE),
              "cleaning stages spend recovery: the tradeoff reappears across passes");
        std::printf("        (best magnetite grade %.3f at pass %d; at pass 256 magnetite %.3f, hematite %.3f)\n",
                    best, best_n, cur.grade(MAGNETITE), cur.grade(HEMATITE));

        const double dens_mag = PHASES[MAGNETITE].density, dens_hem = PHASES[HEMATITE].density;
        check(std::fabs(dens_mag - dens_hem) / dens_hem < 0.05,
              "magnetite and hematite are within 5% on density: no pan can tell them apart");
    }

    // ---- 6. Over-processing, and what actually causes it -------------------
    // A blow both liberates and over-grinds; the two are one act. DESIGN.md
    // claimed the resulting optimum was "a day-one tradeoff with no arbitrary
    // numbers in it." Measured, that is half right, and the half that is wrong
    // is worth more than the half that is right.
    {
        const Substance locked = weathered_outcrop();

        auto sweep = [&](const Substance& ore, double eff, int cycles,
                         double& best_i, double& best_rec, bool& interior) {
            best_i = -1.0; best_rec = -1.0;
            std::vector<double> rec;
            for (double i = 0.0; i <= 0.901; i += 0.05) {
                const Substance p = closed_circuit(ore, i, eff, cycles);
                const double r = recovery(p, separate(p, PAN).concentrate, MAGNETITE);
                rec.push_back(r);
                if (r > best_rec) { best_rec = r; best_i = i; }
            }
            interior = (best_i > 1e-9) && (best_i < 0.899);
            return rec;
        };

        double bi, br; bool interior;

        // A good screen removes finished material before it can be reground, so
        // grinding harder is simply better. This is why closed circuits exist.
        sweep(locked, 1.00, 3, bi, br, interior);
        check(!interior && bi > 0.899, "behind a perfect screen, more grinding is always better");
        sweep(locked, 0.75, 3, bi, br, interior);
        check(!interior && bi > 0.899, "behind a good screen (0.75), more grinding is still always better");

        // Let the screen misplace enough finished material back into the mill and
        // the optimum appears. Its existence, and its location, are set by the
        // screen's efficiency — which is an AUTHORED number. Over-grinding is
        // therefore not free. It is bought.
        sweep(locked, 0.50, 3, bi, br, interior);
        check(interior, "behind a poor screen (0.50), recovery peaks at an interior crush intensity");
        std::printf("        (screen 0.50: recovery peaks %.3f at intensity %.2f)\n", br, bi);
        double bi25, br25; bool int25;
        sweep(locked, 0.25, 3, bi25, br25, int25);
        check(int25 && bi25 < bi, "a worse screen moves the optimum toward less grinding");
        std::printf("        (screen 0.25: recovery peaks %.3f at intensity %.2f)\n", br25, bi25);

        // The parameter-free half, and it is the one worth teaching: crushing
        // material that is already liberated has nothing to liberate. It only
        // makes fines. This holds at every screen efficiency, and no number in
        // this project can be tuned to make it false.
        ScoopBuilder pb;
        const double q[N_SIZE]  = {1.05, 5.25, 0.70}, ql[N_SIZE] = {1.0, 1.0, 1.0};
        const double mg[N_SIZE] = {0.12, 0.60, 0.08}, mgl[N_SIZE] = {0.95, 0.95, 0.95};
        pb.put(MAGNETITE, mg, mgl); pb.put(QUARTZ, q, ql);
        const Substance placer = pb.build();

        bool always_harmful = true;
        for (double eff : {1.00, 0.75, 0.50}) {
            double prev = 1e9;
            for (double i = 0.0; i <= 0.901; i += 0.1) {
                const Substance p = closed_circuit(placer, i, eff, 3);
                const double r = recovery(p, separate(p, PAN).concentrate, MAGNETITE);
                if (r > prev + 1e-9) always_harmful = false;
                prev = r;
            }
        }
        check(always_harmful, "crushing an already-liberated ore only ever destroys recovery");

        // And over-grinding is not a separate failure mode at all: grade climbs
        // monotonically as recovery is spent. It is the same law, on the size
        // axis instead of the density axis. It was implemented once.
        bool grade_climbs = true; double prev_g = -1.0;
        for (double i = 0.0; i <= 0.901; i += 0.05) {
            const Substance p = closed_circuit(locked, i, 0.50, 8);
            const double g = separate(p, PAN).concentrate.grade(MAGNETITE);
            if (g < prev_g - 1e-9) grade_climbs = false;
            prev_g = g;
        }
        check(grade_climbs, "past the recovery optimum, grinding still buys grade: the law again");

        const double r0 = recovery(locked, separate(locked, PAN).concentrate, MAGNETITE);
        check(br > r0, "crushing a locked ore is worth doing at all");
    }

    // ---- The picture -------------------------------------------------------
    std::printf("\n  grade/recovery, magnetite from river sand (feed grade %.3f)\n", feed.grade(MAGNETITE));
    std::printf("  %-8s %8s %10s %8s\n", "tool", "cut", "recovery", "grade");
    for (const SeparatorParams* sp : {&HANDS, &PAN, &SLUICE})
        for (double cut : {3.0, 3.5, 4.0, 4.5}) {
            SeparatorParams q = *sp; q.cut_density = cut;
            const SeparationResult r = separate(feed, q);
            std::printf("  %-8s %8.2f %10.3f %8.3f\n", sp->name, cut,
                        recovery(feed, r.concentrate, MAGNETITE), r.concentrate.grade(MAGNETITE));
        }

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ok", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
