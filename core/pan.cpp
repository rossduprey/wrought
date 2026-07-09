// Phase A, step 1.5: play the pan.
//
// This is not a test. It is the smallest possible answer to the only question
// `core/` cannot answer by passing: is the grade/recovery tradeoff a *game*, or
// is it a spreadsheet? Nobody can settle that by arguing. Somebody has to sit
// down and wash a pan and find out whether they want to do it again.
//
// It runs on exactly the same physics as test_separation.cpp. There is no
// difficulty curve, no balance pass, and no number in here that was chosen to
// make the experience pleasant. The only thing this file adds to the model is
// *legibility* — it turns partition coefficients into a thing you can watch go
// over the lip, because a mechanic the player cannot perceive is not a mechanic,
// it is a wiki entry. (DESIGN.md: a physical detail earns its place if it is
// true, if it creates a decision, and if it is legible without a wiki. The third
// clause is the one this file exists to satisfy.)
//
// Build: make pan

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include "separate.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// The knob.
//
// A wash is one call to separate(). What the player chooses is the cut: how hard
// he swirls, how far he tips the lip. Strength 0 is not a wash at all — it is
// standing still and pouring the cloudy water off the top, which turns out to be
// the same function with the cut set below every grain of sand. Desliming is not
// a second verb. It is this verb, turned down.
//
// The cuts span the sand band (0.020 .. 0.099 m/s, from the phase table via the
// force balance). They are the operator's call, not the tool's, and nothing here
// is tuned: strength 6 washes away everything but gravel because that is what
// washing that hard does.
struct Wash { double cut; const char* verb; const char* feel; };

static const Wash WASHES[] = {
    {0.0012, "let it settle",  "You hold the pan still. The water clears from the top down."},
    {0.0200, "rock it gently", "Barely a motion. The surface hardly breaks."},
    {0.0300, "swirl it",       "A slow circle. Sand walks around the bottom."},
    {0.0450, "swirl steadily", "The pan moves. Grains climb the wall and fall back."},
    {0.0650, "work it hard",   "Water sloshes. You tip the lip and let it run."},
    {0.0900, "wash it out",    "You are being rough with it now."},
    {0.1200, "scour it",       "Almost everything goes. You know this is too much."},
};
static const int N_WASH = (int)(sizeof(WASHES) / sizeof(WASHES[0]));

// ---------------------------------------------------------------------------
// What the character can see.
//
// He has no scale, no assay, no microscope. He has eyes, and iron oxides are
// black and heavy and quartz is pale. Everything reported below is something a
// person crouching in a river could actually observe. The exact numbers exist —
// they are one function call away — and he does not get them.

// Magnetite and hematite are black. Ilmenite is black. Goethite is brown-yellow,
// pyrite is brassy. The character calls the first three "black sand" and does not
// know they are three things. That confusion is the game.
static double black_mass(const Substance& s) {
    return s.phase_mass(MAGNETITE) + s.phase_mass(HEMATITE) + s.phase_mass(ILMENITE);
}

static double bin_mass(const Substance& s, int bin) {
    double m = 0.0;
    for (int p = 0; p < N_PHASE; ++p) m += s.freegrain[p][bin] + s.composite[p][bin];
    return m;
}

static const char* handful(double kg) {
    if (kg <= 0.0005) return "nothing";
    if (kg < 0.010)   return "a smear";
    if (kg < 0.060)   return "a pinch";
    if (kg < 0.200)   return "a spoonful";
    if (kg < 0.600)   return "a handful";
    if (kg < 1.500)   return "a good double handful";
    return "a heavy load";
}

static void look(const Substance& pan) {
    const double m = pan.total_mass();
    if (m < 1e-6) { std::printf("  The pan is empty.\n"); return; }

    const double blk = black_mass(pan) / m;
    const double fin = bin_mass(pan, FINES) / m;
    const double grv = bin_mass(pan, GRAVEL) / m;

    std::printf("  %s of material sits in the pan.\n", handful(m));

    if (grv > 0.25)      std::printf("  It is mostly stones. You can pick them out.\n");
    else if (grv > 0.02) std::printf("  A few pebbles roll about on top.\n");

    if (fin > 0.30)      std::printf("  The water will not clear. Everything is under mud.\n");
    else if (fin > 0.08) std::printf("  A haze of silt hangs above the sand.\n");

    if (blk < 0.02)      std::printf("  Pale sand, and no colour in it worth the name.\n");
    else if (blk < 0.08) std::printf("  Grey. Here and there, a dark grain.\n");
    else if (blk < 0.20) std::printf("  A dark tail gathers at the back of the pan when you tip it.\n");
    else if (blk < 0.45) std::printf("  Black sand, plainly. It sits under the pale stuff and will not move.\n");
    else if (blk < 0.75) std::printf("  The pan is more black than pale. It is heavy in your hands.\n");
    else                 std::printf("  Black, all of it, and it glitters where the sun catches.\n");
}

// The whole point of the file. The player must be able to see what he lost, at
// the moment he loses it, or the tradeoff is invisible and the game is arithmetic.
static void watch_it_go(const Substance& before, const Substance& gone) {
    const double lost = gone.total_mass();
    if (lost < 1e-6) { std::printf("  Nothing leaves the pan.\n"); return; }

    const double had_black = black_mass(before);
    const double lost_black = black_mass(gone);
    const double frac = had_black > 1e-9 ? lost_black / had_black : 0.0;

    const double lost_fines = bin_mass(gone, FINES);
    const double fine_share = lost > 1e-9 ? lost_fines / lost : 0.0;

    if (fine_share > 0.6)
        std::printf("  A slow brown cloud lifts and follows the water out.\n");
    else
        std::printf("  Sand goes over the lip in a grey rush — %s of it.\n", handful(lost));

    if (frac > 0.30)
        std::printf("  ** The cloud glitters. You are pouring the black sand away. **\n");
    else if (frac > 0.10)
        std::printf("  A dark shimmer turns over in the water as it leaves.\n");
    else if (frac > 0.02)
        std::printf("  One or two dark grains ride out on the wave.\n");

    // Fines are, per the force balance, unrecoverable. He should be told once,
    // in the world's own language, not in a tooltip.
    if (lost_fines > 1e-5 && lost_black > 1e-9 && bin_mass(gone, FINES) > 0.5 * lost_black)
        std::printf("  Whatever is in that mud is not coming back. It is too fine to catch.\n");
}

// ---------------------------------------------------------------------------
// The ground.

struct ScoopBuilder {
    Substance s;
    double demand[N_SIZE] {}, authored[N_SIZE] {};
    void put(int p, const double m[N_SIZE], const double lib[N_SIZE]) {
        constexpr double f = COMPOSITE_TARGET_FRACTION;
        if (p == GANGUE) { for (int k = 0; k < N_SIZE; ++k) authored[k] += m[k]; return; }
        for (int k = 0; k < N_SIZE; ++k) {
            s.freegrain[p][k] += m[k] * lib[k];
            s.composite[p][k] += m[k] * (1.0 - lib[k]) / f;
            demand[k] += (1.0 - f) * s.composite[p][k];
        }
    }
    Substance build() {
        for (int k = 0; k < N_SIZE; ++k) s.freegrain[GANGUE][k] = std::fmax(0.0, authored[k] - demand[k]);
        return s;
    }
};

// Two kilos in a pan is about right for a person's arms.
static Substance river_sand() {
    ScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {2.0*frac*0.15, 2.0*frac*0.75, 2.0*frac*0.10};
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

// The inside of the bend, where the river has already done a pass for you.
static Substance black_sand_bar() {
    ScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {2.0*frac*0.10, 2.0*frac*0.88, 2.0*frac*0.02};
        const double l[N_SIZE] = {lf, ls, lg};
        b.put(p, m, l);
    };
    row(MAGNETITE, 0.220, 0.97, 0.95, 0.70);
    row(ILMENITE,  0.060, 0.95, 0.92, 0.60);
    row(HEMATITE,  0.020, 0.92, 0.88, 0.50);
    row(FELDSPAR,  0.060, 0.95, 0.90, 0.75);
    row(QUARTZ,    0.640, 1.00, 1.00, 1.00);
    return b.build();
}

// A struck rock. The iron is here and none of it is loose.
static Substance weathered_outcrop() {
    ScoopBuilder b;
    const double q[N_SIZE] = {0.04, 0.30, 1.06}, ql[N_SIZE] = {1, 1, 1};
    const double f[N_SIZE] = {0.01, 0.06, 0.20}, fl[N_SIZE] = {0.90, 0.60, 0.30};
    const double m[N_SIZE] = {0.004, 0.03, 0.26}, ml[N_SIZE] = {0.85, 0.35, 0.10};
    const double h[N_SIZE] = {0.002, 0.01, 0.024}, hl[N_SIZE] = {0.80, 0.30, 0.10};
    b.put(FELDSPAR, f, fl); b.put(MAGNETITE, m, ml); b.put(HEMATITE, h, hl); b.put(QUARTZ, q, ql);
    return b.build();
}

// ---------------------------------------------------------------------------
// Time, and the birds.
//
// Per DESIGN.md 2a, a pan is one of the processes that already runs at 1:1 —
// three to five minutes, all of it decision. So the clock here is honest and it
// is not compressed. It exists so the player can notice, at the end, that a good
// concentrate cost him twenty minutes of river, which is exactly what it costs.

static int elapsed = 0;   // seconds
static int ambient_at = 90;

static const char* AMBIENT[] = {
    "  A bird you do not know the name of says the same thing four times.",
    "  The water is very cold above the wrist and not cold at all below it.",
    "  Something moves in the willow behind you. You do not turn around.",
    "  Rain starts, stops, and starts again without committing to it.",
    "  Your knees have gone numb against the gravel.",
    "  Downstream, the river makes a noise like a page turning.",
    "  The sun goes behind something and the black sand stops glittering.",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void spend(int seconds) {
    elapsed += seconds;
    if (elapsed >= ambient_at) {
        std::printf("\n%s\n", AMBIENT[(elapsed / 90) % N_AMBIENT]);
        ambient_at = elapsed + 90 + (elapsed % 70);
    }
}

static void clock() {
    std::printf("  [%d min %02d s at the river]\n", elapsed / 60, elapsed % 60);
}

// ---------------------------------------------------------------------------

static void help() {
    std::printf(
        "\n  dig river | dig bar | dig rock   take a fresh pan of ground\n"
        "  look                              look into the pan\n"
        "  wash <0-6> [hands]                the one verb. 0 pours off the mud;\n"
        "                                    6 pours off almost everything.\n"
        "                                    add 'hands' to use cupped hands, not the pan\n"
        "  pick                              pick the stones out by hand. Slow.\n"
        "  crush                             rock against rock. Frees what is locked.\n"
        "  keep                              tip the pan into the poke and see what you have\n"
        "  quit\n\n");
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    std::printf(
        "\n  You are kneeling in a river.\n"
        "\n"
        "  You have a fired clay pan, and hands. The ground here is sand, and\n"
        "  somewhere in it is the black, heavy stuff that a fire will turn into\n"
        "  iron. Most of what you scoop up is quartz. It always will be.\n"
        "\n"
        "  Nobody will tell you the numbers. Nobody ever did.\n");
    help();

    Substance pan = river_sand();
    Substance kept;   // the poke: everything you decided was good enough
    Substance origin = pan;   // for the honest accounting at the end
    bool crushed_warning = false;

    std::printf("  You scoop up river sand.\n");
    look(pan);

    char line[256];
    for (;;) {
        std::printf("\n> ");
        std::fflush(stdout);
        if (!std::fgets(line, sizeof(line), stdin)) break;
        std::printf("\n");

        std::string cmd(line);
        while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' ')) cmd.pop_back();
        if (cmd.empty()) continue;

        if (cmd == "quit" || cmd == "q") break;
        if (cmd == "help" || cmd == "?") { help(); continue; }
        if (cmd == "look" || cmd == "l") { look(pan); clock(); continue; }

        if (cmd.rfind("dig", 0) == 0) {
            if (pan.total_mass() > 1e-6)
                std::printf("  You tip the pan out. Whatever was in it goes back to the river.\n");
            if (cmd.find("bar") != std::string::npos)       { pan = black_sand_bar();    std::printf("  You dig into the inside of the bend, where the sand is dark.\n"); }
            else if (cmd.find("rock") != std::string::npos) { pan = weathered_outcrop(); std::printf("  You break a lump off the outcrop and drop it in the pan.\n"); }
            else                                            { pan = river_sand();        std::printf("  You scoop up river sand.\n"); }
            origin = pan;
            spend(60);
            look(pan);
            continue;
        }

        if (cmd == "pick") {
            if (bin_mass(pan, GRAVEL) < 1e-5) { std::printf("  There is nothing in there big enough to pick up.\n"); continue; }
            // Cobbing: high grade, near-zero throughput, coarse liberated material
            // only. Exactly the Era 0 verb, and by hand it is not clean.
            const ScreenResult r = screen(pan, 0.85, GRAVEL);
            const double thrown = r.oversize.total_mass();
            pan = r.undersize;
            std::printf("  You pick the stones out one at a time and drop them in the river.\n");
            std::printf("  %s of stone goes. Some sand goes with it, stuck to your fingers.\n", handful(thrown));
            spend(150);
            look(pan);
            continue;
        }

        if (cmd == "crush") {
            if (bin_mass(pan, GRAVEL) < 1e-5) { std::printf("  There is nothing left to break.\n"); continue; }
            pan = crush(pan, 0.35);
            std::printf("  You put a stone on a stone and lean on it until something gives.\n");
            if (!crushed_warning) {
                std::printf("  The water goes cloudy and stays that way.\n");
                crushed_warning = true;
            }
            spend(240);
            look(pan);
            continue;
        }

        if (cmd.rfind("wash", 0) == 0) {
            int n = -1;
            const bool by_hand = cmd.find("hands") != std::string::npos;
            for (size_t i = 4; i < cmd.size(); ++i)
                if (cmd[i] >= '0' && cmd[i] <= '9') { n = cmd[i] - '0'; break; }
            if (n < 0 || n >= N_WASH) { std::printf("  How hard? 0 to %d.\n", N_WASH - 1); continue; }
            if (pan.total_mass() < 1e-6) { std::printf("  The pan is empty.\n"); continue; }

            SeparatorParams sp = by_hand ? HANDS : PAN;
            sp.cut_velocity = WASHES[n].cut;

            const Substance before = pan;
            const SeparationResult r = separate(pan, sp);
            pan = r.concentrate;

            std::printf("  You %s%s.\n", WASHES[n].verb, by_hand ? ", in your cupped hands" : "");
            std::printf("  %s\n", WASHES[n].feel);
            watch_it_go(before, r.tailings);
            spend(n == 0 ? 120 : 40 + n * 10);
            look(pan);
            continue;
        }

        if (cmd == "keep") {
            if (pan.total_mass() < 1e-6) { std::printf("  There is nothing in the pan.\n"); continue; }
            kept.add(pan);
            std::printf("  You tip the pan into the poke.\n\n");

            // The one place the game tells the truth. He earned it: he can weigh
            // the black sand on a stick and a stone, and he can see that it is
            // black. He still cannot tell magnetite from hematite, and neither
            // can any pan that ever existed.
            const double m = kept.total_mass();
            const double blk = black_mass(kept);
            std::printf("  In the poke: %.0f g.\n", m * 1000.0);
            std::printf("  Of that, %.0f g is black sand — %.0f%% of what you kept.\n",
                        blk * 1000.0, 100.0 * blk / m);
            std::printf("  You started this pan with %.0f g of black sand in the ground.\n",
                        black_mass(origin) * 1000.0);
            std::printf("  You have %.0f%% of it.\n", 100.0 * blk / std::fmax(black_mass(origin), 1e-9));

            // Pyrite is heavy and brassy, so a pan concentrates it right alongside
            // the iron oxides and cannot be made to do otherwise. It carries the
            // sulphur that makes a finished bar red-short and crack under the
            // hammer. He is not told that. He is told it is pretty.
            const double py = kept.phase_mass(PYRITE);
            if (py / m > 0.004)
                std::printf("  There are brassy flecks in it that catch the light. They are pretty.\n");

            // And the pale remainder is what will become slag in the bloom.
            std::printf("  The rest is pale sand — %.0f g of it — and the fire will not remove it.\n",
                        (m - blk - py) * 1000.0);

            // A stone falls faster than any grain of sand, whatever it is made of.
            // No amount of washing removes it, and the pan cannot tell him that.
            // The weight in his hands can.
            const double grv = bin_mass(kept, GRAVEL) / m;
            if (grv > 0.20)
                std::printf("  It is heavy for what it is. Most of it is stone you never picked out.\n");

            std::printf("\n  Grade is what you kept. Recovery is what you did not lose.\n");
            std::printf("  You cannot have both. Nobody can.\n");
            clock();

            pan = Substance();
            continue;
        }

        std::printf("  You can't do that here.\n");
    }

    std::printf("\n  You stand up. Your knees hurt.\n");
    if (kept.total_mass() > 1e-6) {
        const double blk = black_mass(kept);
        std::printf("  You are carrying %.0f g of concentrate, %.0f%% black sand,\n",
                    kept.total_mass() * 1000.0, 100.0 * blk / kept.total_mass());
        std::printf("  and it cost you %d minutes.\n", elapsed / 60);
    } else {
        std::printf("  You are carrying nothing.\n");
    }
    std::printf("\n");
    return 0;
}
