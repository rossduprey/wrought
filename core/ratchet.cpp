// Phase A, step 2: walk Era 0 into Era 1 and find out whether the ratchet closes.
//
// DESIGN.md calls the ratchet "the engine of the entire game" and asserts it
// without ever having run it. This is the run. Nothing below is a test; it is a
// measurement, and it is allowed to come out badly.
//
// Build: make ratchet

#include <cstdio>
#include <initializer_list>
#include "fire.h"

using namespace wrought;

// Dirt from the bank. Not river sand -- this is what you dig when you want clay,
// and its clay-sized fraction is mostly, but not entirely, kaolinite.
//
// *(This comment used to end: "The part that is not is the grit you will never get
// out, because a clay-sized quartz grain and a clay-sized kaolinite platelet fall
// through water at speeds that differ by two percent." They differ by 698%. The
// two percent was the difference between their densities, which is all that was
// left once `settling.h` had made them both spheres of one diameter. You get the
// grit out by waiting. -- 2026-07-10, #13.)*
static Substance bank_dirt(double kg = 10.0) {
    Substance s;
    auto put = [&](int p, double frac, const double psd[N_SIZE]) {
        for (int k = 0; k < N_SIZE; ++k) s.freegrain[p][k] = kg * frac * psd[k];
    };
    const double clayish[N_SIZE] = {0.90, 0.10, 0.00, 0.00};
    const double sandy[N_SIZE]   = {0.04, 0.16, 0.60, 0.20};
    const double silty[N_SIZE]   = {0.10, 0.55, 0.35, 0.00};
    put(KAOLINITE, 0.25, clayish);
    put(QUARTZ,    0.55, sandy);
    put(FELDSPAR,  0.15, silty);
    put(CALCITE,   0.05, silty);
    return s;
}

static void report(const char* what, const Substance& feed, const Substance& liquor) {
    if (liquor.total_mass() < 1e-9) { std::printf("  %-28s (nothing poured)\n", what); return; }
    std::printf("  %-28s %6.0f g   clay grade %.4f   clay recovery %.4f   grit %5.2f um\n",
                what, liquor.total_mass() * 1000, clay_grade(liquor),
                recovery(feed, liquor, KAOLINITE), grit_diameter(liquor) * 1e6);
}

int main() {
    const Substance dirt = bank_dirt();

    std::printf("bank dirt: %.1f kg, clay grade %.4f, grit %.2f um\n\n",
                dirt.total_mass(), clay_grade(dirt), grit_diameter(dirt) * 1e6);

    // -- 1. The window ------------------------------------------------------
    // How long must you wait, and how long may you wait? The cut is clear/t.
    std::printf("velocities that bracket the process (m/s):\n");
    std::printf("  silt quartz (reject)  %.3e\n", free_velocity(QUARTZ, SILT));
    std::printf("  clay kaolinite (keep) %.3e   ratio %.0fx\n\n",
                free_velocity(KAOLINITE, CLAY),
                free_velocity(QUARTZ, SILT) / free_velocity(KAOLINITE, CLAY));

    // -- 2. The hollow, against the clock -----------------------------------
    std::printf("levigation in %s (%.0f L):\n", HOLLOW.name, HOLLOW.volume() * 1000);
    for (double t : {30.0, 120.0, 600.0, 3600.0, 4.0*3600, 12.0*3600, 24.0*3600, 72.0*3600}) {
        const DecantResult d = decant(dirt, HOLLOW, t);
        char label[64];
        std::snprintf(label, sizeof label, "wait %5.1f h", t / 3600.0);
        report(label, dirt, d.liquor);
    }

    const double clear = clear_depth(dirt, HOLLOW, 3600.0);
    std::printf("\n  sediment fills %.0f%% of the hollow; you may pour off the rest.\n",
                100.0 * (1.0 - clear / HOLLOW.depth));
    std::printf("  the cut at a 1 h wait is %.2e m/s -- silt is rejected outright.\n\n",
                levigation_cut(clear, 3600.0));

    // -- 3. Does the vessel change the CLAY, or only how much of it? ---------
    std::printf("the same wait (4 h) in vessels of different volume:\n");
    for (const Vessel& v : {Vessel{0.05, 0.20, "shallow hollow"},
                            Vessel{0.20, 0.10, "deep narrow jar"},
                            Vessel{0.05, 0.40, "wide flat pan"},
                            Vessel{0.30, 0.15, "large pot"}}) {
        const DecantResult d = decant(dirt, v, 4.0 * 3600);
        char label[64];
        std::snprintf(label, sizeof label, "%-16s %5.1f L", v.name, v.volume() * 1000);
        report(label, dirt, d.liquor);
    }

    // -- 4. The ratchet, iterated -------------------------------------------
    // Gen -1 is the pot you would have pinched from the dirt as you dug it, if
    // nobody had told you to levigate. It is the control, and it is the only
    // rung on the ladder.
    const double d_cut = cut_diameter(PAN.cut_velocity);
    auto shelter = [&](const Substance& b) { return sheltered_fraction(grit_diameter(b), d_cut); };

    const SeparatorParams raw_pan = fire_pan(dirt);
    std::printf("\nthe ratchet: levigate, fire a pot, levigate in the pot, fire again.\n");
    std::printf("  %-16s %10s %10s %12s %10s %10s %9s\n",
                "gen", "vessel L", "clay grade", "grit um", "sigma", "9^sigma", "shelter");
    std::printf("  %-16s %10s %10.4f %12.1f %10.4f %10.3f %8.1f%%\n",
                "-1 (unlevigated)", "-", clay_grade(dirt), grit_diameter(dirt) * 1e6,
                raw_pan.sharpness, imperfection(raw_pan), 100.0 * shelter(dirt));
    std::printf("  %-16s %10s %10s %12s %10.4f %10.3f %8.1f%%   <- for comparison\n",
                "(cupped hands)", "-", "-", "-", HANDS.sharpness, imperfection(HANDS), 0.0);

    Vessel ves = HOLLOW;
    for (int gen = 0; gen < 6; ++gen) {
        const Substance body = decant(dirt, ves, 4.0 * 3600).liquor;
        if (body.total_mass() < 1e-9) { std::printf("  gen %d: nothing poured\n", gen); break; }
        const SeparatorParams pan = fire_pan(body);
        char label[32]; std::snprintf(label, sizeof label, "%d", gen);
        std::printf("  %-16s %10.2f %10.4f %12.3f %10.4f %10.3f %8.1f%%\n",
                    label, ves.volume() * 1000, clay_grade(body), grit_diameter(body) * 1e6,
                    pan.sharpness, imperfection(pan), 100.0 * shelter(body));
        ves = throw_pot(body);
    }

    // -- 4a. Where the whole ratchet actually lives --------------------------
    // Not between generations. Between "raw dirt" and "one minute of standing
    // water" -- and not because the stones blur the pan. Because they swallow it.
    // A sand grain wedged between stones coarser than itself sits below the height
    // at which the log law gives it any flow, and it does not move at any cut. The
    // sigma column barely notices what the shelter column does.
    //
    // *(2026-07-10, #10. This table had no shelter column, and so it charged the
    // stones to sigma: raw dirt read 1.83, worse than cupped hands, and a test
    // asserted it. A separator has two misplacements. `separate.h` learned that
    // about screens on 2026-07-09; this file was making the same mistake about
    // floors the whole time, one function away.)*
    std::printf("\nwhat the pot is made of, and what the pan it becomes is worth:\n");
    const double vr = free_velocity(MAGNETITE, SAND) / free_velocity(QUARTZ, SAND);
    auto rate = [&](const char* n, const Substance& b) {
        const SeparatorParams p = fire_pan(b);
        std::printf("  %-38s grit %8.1f um  sigma %.4f  shelter %5.1f%%  enrich %.2fx\n",
                    n, grit_diameter(b) * 1e6, p.sharpness, 100.0 * shelter(b),
                    std::pow(vr, 1.0 / p.sharpness));
    };
    std::printf("  %-38s %17s  sigma %.4f  shelter %5.1f%%  enrich %.2fx\n",
                "cupped hands (no pot at all)", "",
                HANDS.sharpness, 0.0, std::pow(vr, 1.0 / HANDS.sharpness));
    rate("pot from raw dirt, stones and all", dirt);
    rate("pot from stone-picked dirt", screen(dirt, HAND_COB).undersize);
    rate("pot from a 1-minute decant", decant(screen(dirt, HAND_COB).undersize, HOLLOW, 60.0).liquor);
    rate("pot from a 1-hour decant", decant(dirt, HOLLOW, 3600.0).liquor);
    rate("pot from a 4-hour decant", decant(dirt, HOLLOW, 14400.0).liquor);
    std::printf("  (the enrichment of a sheltered pot is a fiction: it is what the sand that\n"
                "   still moves would do, and half of it is not moving.)\n");

    // -- 4b. Where the grade ceiling came from, until it stopped being one ---
    // This instrument was written to show that the clay grade was arithmetic on
    // the deposit's clay bin and not a property of any tool, because clay-sized
    // quartz and clay-sized kaolinite fell at the same speed. They do not. They
    // fell at the same speed because both were modelled as spheres of the same
    // diameter, and a kaolinite crystal is a plate. At 6.98x the bin divides, the
    // ceiling is a curve, and levigation reaches pure kaolinite if you wait.
    // *(2026-07-10, #13. The instrument kept its table and lost its point.)*
    std::printf("\nthe clay bin of the dirt, before any process touches it:\n");
    double cb = 0.0;
    for (int p = 0; p < N_PHASE; ++p) cb += dirt.freegrain[p][CLAY];
    for (int p = 0; p < N_PHASE; ++p)
        if (dirt.freegrain[p][CLAY] > 1e-12)
            std::printf("  %-10s %6.1f g  %5.1f%%\n", PHASES[p].id,
                        dirt.freegrain[p][CLAY] * 1000, 100 * dirt.freegrain[p][CLAY] / cb);
    std::printf("  kaolinite/quartz velocity ratio in the clay bin: %.3fx\n",
                free_velocity(QUARTZ, CLAY) / free_velocity(KAOLINITE, CLAY));
    std::printf("  (it was 1.031x while kaolinite was a sphere, and that was the whole\n"
                "   argument for Era 1's lodestone-shaped hole. See issue #15.)\n");

    // -- 4c. What levigation can actually reach, given patience --------------
    std::printf("\nthe grade/recovery curve levigation did not have yesterday:\n");
    std::printf("  %-14s %-10s %-10s %-10s\n", "vessel", "wait", "grade", "recovery");
    const Vessel vessels[2] = {HOLLOW, Vessel{0.30, 0.15, "large pot"}};
    const char* names[2] = {"hollow", "large pot"};
    for (int i = 0; i < 2; ++i)
        for (double t : {3600.0, 14400.0, 57600.0, 230400.0, 921600.0}) {
            const Substance liq = decant(dirt, vessels[i], t).liquor;
            if (liq.total_mass() < 1e-9) continue;
            char wait[16];
            if (t < 86400.0) std::snprintf(wait, sizeof wait, "%.0f h", t / 3600.0);
            else             std::snprintf(wait, sizeof wait, "%.0f d", t / 86400.0);
            std::printf("  %-14s %-10s %-10.4f %-10.4f\n",
                        names[i], wait, liq.grade(KAOLINITE), recovery(dirt, liq, KAOLINITE));
        }

    // -- 5. So what would a better pan actually take? ------------------------
    std::printf("\nwhat sharpness is worth what, magnetite over quartz in sand:\n");
    for (double sig : {1.20, 0.64, 0.56, 0.55, 0.22})
        std::printf("  sigma %.2f   imperfection %6.2f   enrichment %6.2fx\n",
                    sig, std::pow(9.0, sig), std::pow(vr, 1.0 / sig));

    // -- 5a. Not a cleaner floor -------------------------------------------
    // The old version of this block priced a 10% blur at 267 um of grit against a
    // 1061 um skin, and concluded the ratchet was dead by three orders of
    // magnitude. It was dead by more than that, and the skin had nothing to do
    // with it. Roughness broadens the cut by displacing the grain the pan is
    // deciding about -- the one falling at exactly v50 -- and that spread
    // saturates before it can matter. *(2026-07-10, #10.)*
    std::printf("\nwhat roughness can and cannot do to a pan (the grain at the cut is %.0f um):\n",
                d_cut * 1e6);
    std::printf("  %-28s %10s %10s %10s\n", "floor", "d_grit um", "sigma", "shelter");
    const double grits[5] = {bin_diameter(CLAY), bin_diameter(SILT), 1.0e-4, d_cut, 5.0 * d_cut};
    const char* gnames[5] = {"levigated clay (clay grit)", "silt grit", "100 um grit",
                             "grit as coarse as the cut", "cobbles"};
    for (int i = 0; i < 5; ++i)
        std::printf("  %-28s %10.3f %10.4f %9.1f%%\n", gnames[i], grits[i] * 1e6,
                    std::hypot(WRIST_SHARPNESS, QUADRATURE * roughness_spread(grits[i], d_cut)),
                    100.0 * sheltered_fraction(grits[i], d_cut));
    std::printf("\n  a 10%% blur would need sigma 0.605. Roughness tops out at %.4f, and past\n"
                "  that the sand hides between the grit instead of resting on it -- which is a\n"
                "  shelter, not a blur. The coarsest thing levigation can carry over is\n"
                "  clay-sized (%.2f um), and it moves sigma by %.1e.\n",
                std::hypot(WRIST_SHARPNESS, QUADRATURE * roughness_spread(d_cut, d_cut)),
                bin_diameter(CLAY) * 1e6,
                std::hypot(WRIST_SHARPNESS, QUADRATURE * roughness_spread(bin_diameter(CLAY), d_cut))
                    - WRIST_SHARPNESS);
    return 0;
}
