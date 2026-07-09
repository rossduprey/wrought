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
// and its clay-sized fraction is mostly, but not entirely, kaolinite. The part
// that is not is the grit you will never get out, because a clay-sized quartz
// grain and a clay-sized kaolinite platelet fall through water at speeds that
// differ by two percent.
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
    const SeparatorParams raw_pan = fire_pan(dirt);
    std::printf("\nthe ratchet: levigate, fire a pot, levigate in the pot, fire again.\n");
    std::printf("  %-16s %10s %10s %12s %10s %10s\n",
                "gen", "vessel L", "clay grade", "grit um", "sigma", "9^sigma");
    std::printf("  %-16s %10s %10.4f %12.1f %10.4f %10.3f\n",
                "-1 (unlevigated)", "-", clay_grade(dirt), grit_diameter(dirt) * 1e6,
                raw_pan.sharpness, imperfection(raw_pan));
    std::printf("  %-16s %10s %10s %12s %10.4f %10.3f   <- for comparison\n",
                "(cupped hands)", "-", "-", "-", HANDS.sharpness, imperfection(HANDS));

    Vessel ves = HOLLOW;
    for (int gen = 0; gen < 6; ++gen) {
        const Substance body = decant(dirt, ves, 4.0 * 3600).liquor;
        if (body.total_mass() < 1e-9) { std::printf("  gen %d: nothing poured\n", gen); break; }
        const SeparatorParams pan = fire_pan(body);
        char label[32]; std::snprintf(label, sizeof label, "%d", gen);
        std::printf("  %-16s %10.2f %10.4f %12.3f %10.4f %10.3f\n",
                    label, ves.volume() * 1000, clay_grade(body), grit_diameter(body) * 1e6,
                    pan.sharpness, imperfection(pan));
        ves = throw_pot(body);
    }

    // -- 4a. Where the whole ratchet actually lives --------------------------
    // Not between generations. Between "raw dirt" and "one minute of standing
    // water", because sand is the only thing coarse enough to blur a pan and
    // sand is the first thing to fall.
    std::printf("\nwhat the pot is made of, and what the pan it becomes is worth:\n");
    const double vr = free_velocity(MAGNETITE, SAND) / free_velocity(QUARTZ, SAND);
    auto rate = [&](const char* n, const Substance& b) {
        const SeparatorParams p = fire_pan(b);
        std::printf("  %-38s grit %8.1f um  sigma %.4f  enrich %.2fx\n",
                    n, grit_diameter(b) * 1e6, p.sharpness, std::pow(vr, 1.0 / p.sharpness));
    };
    std::printf("  %-38s %17s  sigma %.4f  enrich %.2fx\n", "cupped hands (no pot at all)", "",
                HANDS.sharpness, std::pow(vr, 1.0 / HANDS.sharpness));
    rate("pot from raw dirt, stones and all", dirt);
    rate("pot from stone-picked dirt", screen(dirt, HAND_COB).undersize);
    rate("pot from a 1-minute decant", decant(screen(dirt, HAND_COB).undersize, HOLLOW, 60.0).liquor);
    rate("pot from a 1-hour decant", decant(dirt, HOLLOW, 3600.0).liquor);
    rate("pot from a 4-hour decant", decant(dirt, HOLLOW, 14400.0).liquor);

    // -- 4b. Where the grade ceiling comes from -----------------------------
    // Nowhere in the tool. It is arithmetic on the deposit's clay bin, because
    // clay-sized quartz and clay-sized kaolinite fall at the same speed.
    std::printf("\nthe clay bin of the dirt, before any process touches it:\n");
    double cb = 0.0;
    for (int p = 0; p < N_PHASE; ++p) cb += dirt.freegrain[p][CLAY];
    for (int p = 0; p < N_PHASE; ++p)
        if (dirt.freegrain[p][CLAY] > 1e-12)
            std::printf("  %-10s %6.1f g  %5.1f%%\n", PHASES[p].id,
                        dirt.freegrain[p][CLAY] * 1000, 100 * dirt.freegrain[p][CLAY] / cb);
    std::printf("  kaolinite/quartz velocity ratio in the clay bin: %.3fx\n",
                free_velocity(QUARTZ, CLAY) / free_velocity(KAOLINITE, CLAY));

    // -- 5. So what would a better pan actually take? ------------------------
    std::printf("\nwhat sharpness is worth what, magnetite over quartz in sand:\n");
    for (double sig : {1.20, 0.64, 0.56, 0.55, 0.22})
        std::printf("  sigma %.2f   imperfection %6.2f   enrichment %6.2fx\n",
                    sig, std::pow(9.0, sig), std::pow(vr, 1.0 / sig));

    std::printf("\ngrit needed to blur a pan by 10%% (sigma 0.55 -> 0.605):\n");
    const double s_need = std::sqrt(0.605 * 0.605 - 0.55 * 0.55);
    std::printf("  d_grit = %.0f um, against a skin %.0f um deep.\n",
                s_need * skin_depth() * 1e6, skin_depth() * 1e6);
    std::printf("  the coarsest thing levigation can carry over is clay-sized: %.2f um.\n",
                bin_diameter(CLAY) * 1e6);
    return 0;
}
