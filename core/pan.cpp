// Phase A, step 1.5: play the pan.
//
// This is not a test. It is the smallest possible answer to the only question
// `core/` cannot answer by passing: is the grade/recovery tradeoff a *game*, or
// is it a spreadsheet? Nobody can settle that by arguing. Somebody has to kneel
// down and wash a pan and find out whether they want to do it again.
//
// It runs on exactly the same physics as test_separation.cpp. There is no
// difficulty curve, no balance pass, and no number in here that was chosen to
// make the experience pleasant. What this file adds to the model is *legibility*
// and nothing else. (DESIGN.md: a physical detail earns its place if it is true,
// if it creates a decision, and if it is legible without a wiki.)
//
// -- The second version, and why. -------------------------------------------
//
// The first version was a command parser. You typed `wash 3` and it called
// separate() once with a cut chosen from a table of seven. Ross played it and
// reported, exactly: "learning the interface was most of the work... I am more
// interested in what actions I need to take to win the game than to learn what
// this is teaching me... since there was no persistent display showing me data
// that changed as I worked the pan, I wasn't seeing the consequences of my
// actions in realtime."
//
// He had found a defect in the model, not the presentation. A wash is not one
// call to separate() at a cut the operator picks. **The cut is the instantaneous
// speed of the water**, it changes continuously while the hands move, and the
// only decision a panner actually makes is *when to stop* — which he makes over
// and over, with his eyes, while it is happening. The first version simulated the
// outcome and made the player type it. So of course he optimised for the outcome.
//
// Therefore: separate() is called every tick, at whatever cut the player's hand
// is producing right now, and the pan is redrawn. The table of seven wash
// strengths is deleted. It is replaced by one time constant. That is seven
// authored numbers traded for one, which is the same trade the settling-velocity
// rewrite made, and it is starting to look like a law.
//
// -- The third version, and why. --------------------------------------------
//
// The second version showed weight, colour, and what left the pan, and nothing
// else. The comment that used to sit here said: "He is never shown a grade.
// Grade is an assay, and there is no laboratory in a river. Put a live
// percentage on the screen and he will optimise the percentage and never look
// at the sand again."
//
// That was two claims wearing one coat, and only one of them was true.
//
// The true one: a *score* — one number, with an arrow pointing up — is a thing
// the player optimises instead of playing. That danger is real and it is why
// there is still no score in here, no target, and no win.
//
// The false one: that the composition of the pan is a score. It is not. It is
// the **state**, and the state is the whole claim this project makes. There is
// no `iron ore` item; there is a vector over mineral phases, and if we hide it
// then the one thing that distinguishes this from every other panning minigame
// ever shipped is invisible to the person we built it for. A simulation that
// conceals its state is a video with a keyboard attached.
//
// And "there is no laboratory in a river" was answering a question nobody asked.
// The panner has no assay. The panner is not alone. README claim #3 is a
// co-labouring AI that stands next to you and knows things, and *what is in this
// dirt* is precisely the kind of thing it is for. Asking it is not cheating. It
// is the relationship.
//
// So: the assay panel is live, it is always on, and it shows grade and recovery
// **together**, for every phase, because those are the two halves of the law and
// showing one without the other is what a score would be. Grade rises. Recovery
// only ever falls. Watching those two columns move in opposite directions, in
// real time, under your own hand, is the entire lesson of Era 0 and it took
// eleven lines of printf.
//
//   (Ross, having played v2: "i do not see the contents of the sand, and this is
//   probably the most important part. like asking you for the details of an
//   object... as i pan, the percentages of the minerals should change right in
//   front of me.")
//
// Build: make pan   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <initializer_list>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "separate.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// The hand.
//
// A stroke of the wrist puts energy into the water; drag takes it out again. So
// the cut is not a setting, it is a *state*, and holding it up costs continuous
// effort — which is what swirling a pan is. Tap to swirl. Stop tapping and the
// water goes still and nothing at all leaves the pan, because a cut below every
// grain in the pan is the same call to separate() with a different number in it.
// Desliming is not a verb. It is swirling gently, and you will find that out by
// doing it.

static constexpr double DT           = 0.05;   // s per tick
static constexpr double STROKE       = 0.032;  // m/s of water per wrist stroke. AUTHORED.
static constexpr double DRAG_TAU     = 0.90;   // s. the water settles this fast. AUTHORED.
static constexpr double PASS_TAU     = 1.50;   // s for one separation "pass". AUTHORED.
static constexpr double MAX_CUT      = 0.170;  // you cannot swirl harder than this
static constexpr double PICK_SECONDS = 6.0;

// A partition coefficient is a per-pass probability. Panning is continuous, so a
// particle survives dt of washing with probability P^(dt/PASS_TAU). Integrate
// that and you get, for free, the thing test_separation.cpp needed 256 discrete
// passes to show: wash long enough and the pan starts throwing your magnetite
// away in favour of the hematite, because it is very slightly faster.
//
// Note what PASS_TAU can and cannot do. Every particle's retention depends on the
// elapsed time only through t/PASS_TAU, so the *path* the pan traces through
// (grade, recovery) is completely independent of it. PASS_TAU sets how fast your
// hands are. It cannot move the tradeoff by one part in a million. That is the
// only kind of number this project is allowed to author freely, and it is worth
// knowing which numbers those are.
static Substance wash_tick(Substance& pan, double cut, Substance& out_lost) {
    SeparatorParams sp = PAN;
    // Still water carries nothing over the lip. As the cut goes to zero the
    // partition goes to one for every particle, so this clamp must sit below the
    // slowest grain in the table (charcoal fines, 5.8e-5 m/s) by a wide margin --
    // not merely at some small number. Put it in the fines band by accident and
    // the pan bleeds silt while the player sits perfectly still.
    sp.cut_velocity = (cut < 1e-9) ? 1e-9 : cut;
    const double e = DT / PASS_TAU;

    // Only the skin is in the water. The rest is bed, and the bed is safe. The
    // mixing scale is the cut itself: how hard you are swirling is how deep you
    // are stirring. Nothing else about the pan's geometry enters the wash.
    const Substance top = exposed(pan, cut, skin_mass());

    Substance gone;
    const double T = pan.temperature;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double rf = std::pow(partition(free_velocity(p, s, T), sp), e);
            const double rc = std::pow(partition(composite_velocity(p, s, T), sp), e);
            gone.freegrain[p][s] = top.freegrain[p][s] * (1.0 - rf);
            gone.composite[p][s] = top.composite[p][s] * (1.0 - rc);
            pan.freegrain[p][s] -= gone.freegrain[p][s];
            pan.composite[p][s] -= gone.composite[p][s];
        }
    out_lost = gone;
    return gone;
}

// ---------------------------------------------------------------------------
// What the character can see. He has eyes and a pair of arms. Iron oxides are
// black and heavy, quartz is pale. Everything below is something a person
// crouching in a river could actually observe. The exact numbers exist, one
// function call away, and he does not get them.

static double black_mass(const Substance& s) {
    return s.phase_mass(MAGNETITE) + s.phase_mass(HEMATITE) + s.phase_mass(ILMENITE);
}
static double bin_mass(const Substance& s, int bin) {
    double m = 0.0;
    for (int p = 0; p < N_PHASE; ++p) m += s.freegrain[p][bin] + s.composite[p][bin];
    return m;
}
// What a panner calls mud. He cannot see the 3.9 um line and does not care where
// it is; the pan cannot separate across it either. Only levigation can.
static double mud_mass(const Substance& s) { return bin_mass(s, CLAY) + bin_mass(s, SILT); }
static double black_frac(const Substance& s) {
    const double m = s.total_mass();
    return m > 1e-9 ? black_mass(s) / m : 0.0;
}

// Iron that is glued to quartz. It is in the pan, it will not come out, and no
// amount of washing will change that -- only a rock will. This is the grade
// ceiling, and it is the only number in the panel that argues for a verb the
// player does not have yet.
static double locked_black(const Substance& s) {
    constexpr double f = COMPOSITE_TARGET_FRACTION;
    double t = 0.0;
    for (int p : {MAGNETITE, HEMATITE, ILMENITE})
        for (int b = 0; b < N_SIZE; ++b) t += f * s.composite[p][b];
    return t;
}

static const char* colour_word(double bf) {
    if (bf < 0.02) return "pale";
    if (bf < 0.08) return "grey";
    if (bf < 0.20) return "grey, with a dark tail";
    if (bf < 0.45) return "dark";
    if (bf < 0.75) return "black under the pale";
    return "black, and it glitters";
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

// Kaolinite is the one phase whose size is not free to be anything. A clay
// mineral is a platelet a micron across; that is what makes it clay, and it is
// why levigation exists. Every other phase is distributed across the bins by
// what the river did to it. Quartz that happens to be clay-sized is the grit you
// will never wash out of your pot.
static const double CLAY_PSD[N_SIZE]  = {0.90, 0.10, 0.00, 0.00};

static Substance river_sand() {
    ScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {2.0*frac*0.03, 2.0*frac*0.12, 2.0*frac*0.75, 2.0*frac*0.10};
        const double l[N_SIZE] = {lf, lf, ls, lg};
        b.put(p, m, l);
    };
    auto clay_row = [&](int p, double frac) {
        double m[N_SIZE]; const double l[N_SIZE] = {1.0, 1.0, 1.0, 1.0};
        for (int k = 0; k < N_SIZE; ++k) m[k] = 2.0 * frac * CLAY_PSD[k];
        b.put(p, m, l);
    };
    row(FELDSPAR,  0.100, 0.95, 0.90, 0.75);
    clay_row(KAOLINITE, 0.040);
    row(MAGNETITE, 0.080, 0.95, 0.88, 0.55);
    row(HEMATITE,  0.010, 0.90, 0.80, 0.50);
    row(GOETHITE,  0.020, 0.90, 0.80, 0.50);
    row(ILMENITE,  0.030, 0.92, 0.85, 0.55);
    row(PYRITE,    0.005, 0.85, 0.80, 0.50);
    row(CALCITE,   0.015, 0.90, 0.85, 0.60);
    row(QUARTZ,    0.700, 1.00, 1.00, 1.00);
    return b.build();
}

static Substance black_sand_bar() {
    ScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {2.0*frac*0.01, 2.0*frac*0.09, 2.0*frac*0.88, 2.0*frac*0.02};
        const double l[N_SIZE] = {lf, lf, ls, lg};
        b.put(p, m, l);
    };
    row(MAGNETITE, 0.220, 0.97, 0.95, 0.70);
    row(ILMENITE,  0.060, 0.95, 0.92, 0.60);
    row(HEMATITE,  0.020, 0.92, 0.88, 0.50);
    row(FELDSPAR,  0.060, 0.95, 0.90, 0.75);
    row(QUARTZ,    0.640, 1.00, 1.00, 1.00);
    return b.build();
}

static Substance weathered_outcrop() {
    ScoopBuilder b;
    const double q[N_SIZE] = {0.008, 0.032, 0.30, 1.06}, ql[N_SIZE] = {1, 1, 1, 1};
    const double f[N_SIZE] = {0.002, 0.008, 0.06, 0.20}, fl[N_SIZE] = {0.90, 0.90, 0.60, 0.30};
    const double m[N_SIZE] = {0.0008, 0.0032, 0.03, 0.26}, ml[N_SIZE] = {0.85, 0.85, 0.35, 0.10};
    const double h[N_SIZE] = {0.0004, 0.0016, 0.01, 0.024}, hl[N_SIZE] = {0.80, 0.80, 0.30, 0.10};
    b.put(FELDSPAR, f, fl); b.put(MAGNETITE, m, ml); b.put(HEMATITE, h, hl); b.put(QUARTZ, q, ql);
    return b.build();
}

// ---------------------------------------------------------------------------
// Terminal.

static struct termios saved_tty;
static bool tty_raw = false;

static void restore_tty() {
    if (tty_raw) { tcsetattr(STDIN_FILENO, TCSANOW, &saved_tty); tty_raw = false; }
    std::printf("\033[?25h");   // cursor back on
    std::fflush(stdout);
}
static void on_signal(int) { restore_tty(); std::_Exit(130); }

static void raw_tty() {
    tcgetattr(STDIN_FILENO, &saved_tty);
    struct termios t = saved_tty;
    t.c_lflag &= ~(unsigned)(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    tty_raw = true;
    std::atexit(restore_tty);
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::printf("\033[?25l");   // cursor off
}

static int poll_key() {
    char c;
    return (read(STDIN_FILENO, &c, 1) == 1) ? (unsigned char)c : -1;
}
static void nap(double seconds) {
    struct timespec ts;
    ts.tv_sec  = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    nanosleep(&ts, nullptr);
}

// ---------------------------------------------------------------------------
// The pan, drawn.
//
// This is the persistent display, and it is made of dirt rather than numbers.
// The bottom of the pan fills with '#' as the black sand concentrates. That is a
// live grade readout that never says a number, and it is the only one he gets.

static const int PAN_W = 30;
static const int PANEL_COL = 42;

// Which phases this scoop actually contains, heaviest first. Fixed at the moment
// you dig, and never re-sorted -- a table whose rows swap places while you read
// them is a table you cannot read. The order is information too: it is what the
// ground gave you, and it does not change just because you washed it.
struct Assay {
    int order[N_PHASE];
    int n = 0;
};

static Assay assay_of(const Substance& s) {
    Assay a;
    for (int p = 0; p < N_PHASE; ++p)
        if (s.phase_mass(p) > 1e-6) a.order[a.n++] = p;
    for (int i = 1; i < a.n; ++i) {
        const int k = a.order[i];
        int j = i - 1;
        while (j >= 0 && s.phase_mass(a.order[j]) < s.phase_mass(k)) { a.order[j+1] = a.order[j]; --j; }
        a.order[j+1] = k;
    }
    return a;
}

// Grade and recovery, side by side, one row per mineral. Neither column is a
// score, because they move in opposite directions and you cannot have both. That
// sentence is the design of the whole game and here it is as two columns of
// printf.
static void draw_assay(const Substance& pan, const Substance& origin,
                       const Assay& a, const char* arrow) {
    const double m = pan.total_mass();
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("what the pan holds");
    line("%-10s %6s %7s %6s", "", "grams", "grade", "kept");
    line("%s", "----------------------------------");

    for (int i = 0; i < a.n; ++i) {
        const int p = a.order[i];
        const double mp = pan.phase_mass(p);
        const double o  = origin.phase_mass(p);
        if (mp * 1000.0 < 0.05 && o > 1e-6) {
            line("%-10s %6s %7s %6s %c", PHASES[p].id, "-", "-", "gone", ' ');
            continue;
        }
        line("%-10s %6.1f %6.1f%% %5.0f%% %c",
             PHASES[p].id, mp * 1000.0,
             m > 1e-9 ? 100.0 * mp / m : 0.0,
             o > 1e-9 ? 100.0 * mp / o : 0.0,
             arrow[p]);
    }

    const double blk = black_mass(pan), blk0 = black_mass(origin);
    const double lok = locked_black(pan);
    line("%s", "----------------------------------");
    line("%-10s %6.1f %6.1f%% %5.0f%%", "black sand", blk * 1000.0,
         m > 1e-9 ? 100.0 * blk / m : 0.0,
         blk0 > 1e-9 ? 100.0 * blk / blk0 : 0.0);
    if (lok * 1000.0 > 0.05)
        line("  %.1f g of it is locked in quartz", lok * 1000.0);
    line("  washed away: %.0f g", (origin.total_mass() - m) * 1000.0);
}

static const char* AMBIENT[] = {
    "a bird you do not know the name of says the same thing four times",
    "the water is very cold above the wrist and not cold at all below it",
    "something moves in the willow behind you; you do not turn around",
    "rain starts, stops, and starts again without committing to it",
    "your knees have gone numb against the gravel",
    "downstream, the river makes a noise like a page turning",
    "the sun goes behind something and the sand stops glittering",
    "a stick goes past, faster than you expect",
    "the cold has reached the bone of your thumb",
    "somewhere behind you the fire is going out",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

struct Plume { double mass, black, fines; };

static void draw(const Substance& pan, const Substance& origin, const Assay& assay,
                 const char* arrow, double cut, double t,
                 const Plume plume[], int plume_n, int plume_head,
                 const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");

    const double m  = pan.total_mass();
    const double bf = black_frac(pan);
    const double gr = m > 1e-9 ? bin_mass(pan, GRAVEL) / m : 0.0;
    const double fn = m > 1e-9 ? mud_mass(pan) / m : 0.0;

    std::printf("\n   %-46s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);

    // the pan, in section. black sand lies under the pale, because it is heavier.
    const int fill  = (int)std::lround(PAN_W * std::fmin(1.0, m / 2.0));
    const int black = (int)std::lround(fill * bf);

    std::printf("   \\%s/\n", std::string(PAN_W + 2, '_').c_str());
    for (int row = 0; row < 3; ++row) {
        std::printf("    |");
        for (int i = 0; i < PAN_W; ++i) {
            const bool in = i >= (PAN_W - fill) / 2 && i < (PAN_W - fill) / 2 + fill;
            if (!in) { std::putchar(' '); continue; }
            if (row == 2)                     std::putchar(i < (PAN_W - fill) / 2 + black ? '#' : '.');
            else if (row == 1 && fill > 8)    std::putchar(i < (PAN_W - fill) / 2 + black / 2 ? '#' : '.');
            else                              std::putchar(gr > 0.15 && (i % 5) == 0 ? 'o' : (fn > 0.20 ? '~' : ' '));
        }
        std::printf("|\n");
    }
    std::printf("    \\%s/\n\n", std::string(PAN_W, '_').c_str());

    // what is leaving, drifting right, this second and the last two.
    for (int k = 0; k < 3; ++k) {
        const Plume& p = plume[(plume_head - k - 1 + plume_n) % plume_n];
        std::printf("      ");
        if (p.mass < 1e-7) { std::printf("\n"); continue; }
        const int n = (int)std::lround(std::fmin(20.0, p.mass * 4000.0));
        const char ch = p.fines > 0.5 ? '~' : '.';
        for (int i = 0; i < k * 4; ++i) std::putchar(' ');
        for (int i = 0; i < n; ++i) std::putchar(p.black > 0.25 ? '*' : (p.black > 0.08 ? '+' : ch));
        std::printf("\n");
    }

    std::printf("\n   the water: ");
    const int bars = (int)std::lround(cut / MAX_CUT * 24.0);
    for (int i = 0; i < 24; ++i) std::putchar(i < bars ? '=' : ' ');
    if (cut < 0.002) std::printf("  still");
    std::printf("\n");
    std::printf("   the pan:   %s\n", colour_word(bf));
    std::printf("   the weight:%s%.0f g\n", " ", m * 1000.0);
    if (gr > 0.10) std::printf("   there are stones in it.\n");

    std::printf("\n   %s\n", nag ? nag : "");
    std::printf("\n   [space] swirl   [p] pick a stone   [k] keep it   [n] new pan   [q] quit\n");

    draw_assay(pan, origin, assay, arrow);
    std::printf("\033[23;1H");
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "pan: needs a terminal. Run it yourself: cd core && make pan\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You are kneeling in a river.\n\n"
        "   You have a fired clay pan. The ground here is sand, and somewhere in it\n"
        "   is the black, heavy stuff that a fire will turn into iron. Most of what\n"
        "   you scoop up is quartz. It always will be.\n\n"
        "   Swirling is not a keypress. It is a motion you keep up. Tap [space]\n"
        "   over and over to hold the water moving; the moment you stop, it dies,\n"
        "   and a still pan separates nothing. How fast you tap is how hard the\n"
        "   water runs. What that is worth, nobody is going to tell you.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    Substance pan = river_sand(), origin = pan, kept;
    double cut = 0.0, t = 0.0;
    int deposit = 0;

    // Is this mineral's share of the pan rising or falling right now? Sampled on
    // a slow clock, because an arrow that flickers is an arrow nobody reads.
    Assay assay = assay_of(origin);
    char arrow[N_PHASE] = {};
    double grade_was[N_PHASE] = {}, arrow_t = 0.0;
    for (int p = 0; p < N_PHASE; ++p) { arrow[p] = ' '; grade_was[p] = origin.grade(p); }

    Plume plume[3] = {};
    int plume_head = 0;
    double plume_acc_m = 0, plume_acc_b = 0, plume_acc_f = 0, plume_t = 0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag;
    double nag_t = 0;

    // The one thing the game must teach before anything else can be learned, and
    // the one thing it silently failed to teach: a swirl is sustained. A terminal
    // cannot see a held key, so the verb had to be encoded as an impulse, and the
    // encoding lies about what the hand is doing. Until the input device can
    // report a held gesture, the game says it out loud.
    long strokes = 0;
    bool taught = false;

    bool running = true, quit = false;
    while (running) {
        // ---- input ---------------------------------------------------------
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == ' ')       { cut = std::fmin(MAX_CUT, cut + STROKE); ++strokes; }
            else if (c == 'q')  { running = false; quit = true; }
            else if (c == 'n')  {
                deposit = (deposit + 1) % 3;
                pan = deposit == 0 ? river_sand() : deposit == 1 ? black_sand_bar() : weathered_outcrop();
                origin = pan; cut = 0;
                assay = assay_of(origin);
                for (int p = 0; p < N_PHASE; ++p) { arrow[p] = ' '; grade_was[p] = origin.grade(p); }
                nag = deposit == 0 ? "You scoop up river sand."
                    : deposit == 1 ? "You dig into the inside of the bend, where the sand is dark."
                                   : "You break a lump off the outcrop and drop it in the pan.";
                nag_t = t + 4;
            }
            else if (c == 'p') {
                if (bin_mass(pan, GRAVEL) < 1e-5) { nag = "There is nothing in there big enough to pick up."; nag_t = t + 3; }
                else {
                    // Cobbing. You miss a lot of stones and you drop almost no
                    // sand, so you will be doing this again in a minute.
                    pan = screen(pan, HAND_COB).undersize;
                    t += PICK_SECONDS;
                    nag = "You pick out the stones you can see. There will be more.";
                    nag_t = t + 4;
                }
            }
            else if (c == 'k') {
                if (pan.total_mass() < 1e-6) { nag = "There is nothing in the pan."; nag_t = t + 3; }
                else { kept.add(pan); running = false; }
            }
        }

        // ---- the world -----------------------------------------------------
        Substance lost;
        wash_tick(pan, cut, lost);
        cut *= std::exp(-DT / DRAG_TAU);
        t += DT;

        // accumulate the plume over a fifth of a second so it is visible
        plume_acc_m += lost.total_mass();
        plume_acc_b += black_mass(lost);
        plume_acc_f += mud_mass(lost);
        plume_t += DT;
        if (plume_t >= 0.20) {
            plume[plume_head] = {plume_acc_m,
                                 plume_acc_m > 1e-9 ? plume_acc_b / plume_acc_m : 0.0,
                                 plume_acc_m > 1e-9 ? plume_acc_f / plume_acc_m : 0.0};
            plume_head = (plume_head + 1) % 3;
            if (plume_acc_b > 0.20 * plume_acc_m && plume_acc_m > 1e-5 && t > nag_t) {
                nag = "The cloud glitters. You are pouring it away."; nag_t = t + 2.5;
            } else if (plume_acc_f > 0.6 * plume_acc_m && plume_acc_m > 1e-5 && t > nag_t) {
                nag = "Whatever is in that mud is not coming back. It is too fine to catch."; nag_t = t + 4;
            }
            plume_acc_m = plume_acc_b = plume_acc_f = plume_t = 0;
        }

        if (strokes >= 12) taught = true;
        if (!taught && t > 8.0 && cut < 0.004 && pan.total_mass() > 0.9 * origin.total_mass() && t > nag_t) {
            nag = "The water has gone still, and a still pan separates nothing. Keep tapping.";
            nag_t = t + 4;
        }

        if (t >= arrow_t) {
            for (int p = 0; p < N_PHASE; ++p) {
                const double g = pan.grade(p), d = g - grade_was[p];
                arrow[p] = d > 1e-4 ? '^' : d < -1e-4 ? 'v' : ' ';
                grade_was[p] = g;
            }
            arrow_t = t + 0.5;
        }

        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 37) % N_AMBIENT]; ambient_t = t + 37; }
        if (t > nag_t) nag.clear();

        draw(pan, origin, assay, arrow, cut, t, plume, 3, plume_head,
             ambient, nag.empty() ? nullptr : nag.c_str());
        nap(DT);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || kept.total_mass() < 1e-6) {
        std::printf("   You stand up. Your knees hurt. You are carrying nothing.\n\n");
        return 0;
    }

    // The one place the game tells the truth, and only what a scale and an eye
    // can tell him. He still cannot separate magnetite from hematite, and neither
    // can any pan that has ever existed.
    const double m = kept.total_mass(), blk = black_mass(kept), py = kept.phase_mass(PYRITE);
    std::printf("   You tip the pan into the poke.\n\n");
    std::printf("   In the poke: %.0f g.\n", m * 1000.0);
    std::printf("   Of that, %.0f g is black sand — %.0f%% of what you kept.\n", blk * 1000.0, 100.0 * blk / m);
    std::printf("   You started with %.0f g of it in the ground. You have %.0f%%.\n",
                black_mass(origin) * 1000.0, 100.0 * blk / std::fmax(black_mass(origin), 1e-9));
    if (py / m > 0.004)
        std::printf("   There are brassy flecks in it that catch the light. They are pretty.\n");
    std::printf("   The rest is pale sand — %.0f g — and the fire will not remove it.\n", (m - blk - py) * 1000.0);
    if (bin_mass(kept, GRAVEL) / m > 0.20)
        std::printf("   It is heavy for what it is. Most of it is stone you never picked out.\n");
    std::printf("\n   Grade is what you kept. Recovery is what you did not lose.\n");
    std::printf("   You cannot have both. Nobody can.\n\n");
    std::printf("   It cost you %d minutes and %d seconds at the river.\n\n", (int)t / 60, (int)t % 60);
    return 0;
}
