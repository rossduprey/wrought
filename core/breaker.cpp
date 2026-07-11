// Phase A, the third playable slice: break the rock.
//
// pan.cpp asked whether a wash is a game; dig.cpp asked whether a dig is. This
// file takes what the dig hands you -- LOCKED ROCK, a grade you can read and
// cannot keep -- and asks the question the whole front of the chain has been
// building toward: is FREEING the ore a game, or is it a button that turns rock
// into metal?
//
// It runs on exactly the verbs the core already had -- geology.dig_column() for
// the feed, separate.h crush() for the hammer, separate.h separate() for the pan
// -- and adds nothing but a gesture and a face. No number in here was chosen to
// make it pleasant; there is no score, no target, no win.
//
// -- The finding this slice is built on, and the claim it retired. -----------
//
// dig.cpp and the docs said the chain was `dig -> crush -> COB -> furnace`, and
// that cobbing (separate.h HAND_COB) "finally has a job" breaking the locked rock
// apart. A probe (2026-07-11) refuted it, twice over:
//
//   1. Cobbing is a SIZE screen. The dig delivers ore and gangue at the SAME size
//      -- both in the GRAVEL bin (geology.h sample fills composite[mineral][GRAVEL]
//      and free QUARTZ[GRAVEL]). A size screen on a same-size feed enriches
//      NOTHING: it keeps ~60% of ore and ~60% of waste alike.
//   2. Worse, AFTER crushing the freed grains fall to SAND and finer (crush drives
//      mass one bin down), while cob keeps GRAVEL -- so cob-after-crush throws the
//      freed ore away. Grade and recovery both fall with every blow.
//
// The verb that actually parts freed ore from freed gangue is the PAN: it cuts on
// velocity, and a copper grain falls faster than a quartz grain of its size. The
// honest chain is `dig -> crush (liberate) -> pan (density)`, and it is a game:
//
//      blows   liberation   pan grade   pan recovery   lost to fines
//        0         0%          --            0%             0 g      all still locked
//        3        51%         12%           26%           226 g      recovery PEAK
//       12        91%         20%            8%           742 g      ground to dust, over the lip
//
// Under-crush and the ore is still locked, so the pan wins nothing. Over-crush and
// the ore is powder the pan cannot hold -- it washes over the lip. Grade climbs
// the whole way; recovery peaks and falls. That is the project's grade/recovery
// law (RUNBOOK: reported together, never as a score) appearing a seventh time, now
// at the breaker, and the decision the slice exists to make legible is exactly
// WHEN TO STOP HAMMERING. It is never stated, only felt across a few attempts.
//
// History says the same thing, and names the parts: the Cornish floors ran
// spalling -> cobbing -> bucking -> jigging (Cornish Mining WHS; 911Metallurgist).
// Cobbing was hand-sorting the coarse ROCK by eye, a PRE-crush pass done because a
// person can SEE which lump is ore -- a perception, not a size cut, and it comes
// BEFORE the hammer, not after. Bucking is our crush; jigging is our pan. We do
// not model the by-eye cob as a player verb: sorting a pile lump by lump wants a
// whole client interaction that lives outside the window of playability (decided
// with Ross, 2026-07-11). So the modeled breaker is bucking -> jigging.
//
// -- The gesture. -----------------------------------------------------------
//
// The pan taught that a persistent display made of the MATERIAL (a section of
// sand) beats a display made of numbers, and the dig taught that one strike is
// one impulse ([space]) because a terminal cannot see a held button. The breaker
// keeps both. You strike the rock and it breaks; the persistent display is the
// rock itself -- a liberation bar (how much ore is freed) over a grind profile
// (how coarse the pile still is, gravel down to dust). Both are things the eye can
// actually read off a heap of crushed rock; NEITHER is a projected pan result,
// because showing "recovery is falling now" would state the very wisdom the player
// is here to learn. You watch the rock turn to powder, you decide it is enough,
// and you pan it -- and only then do you learn what the choice cost you.
//
// Build: make breaker   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "geology.h"
#include "separate.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// The hammers, and how hard they hit. These are the breaker's counterpart to the
// pan's STROKE and the dig's HAND_STEP: authored rates that set the pace of the
// work and NOTHING a test or a finding reads. `intensity` is passed straight to
// crush(); the probe swept it and the sweet spot lands near three heavy blows.
// A light tap liberates gently and is hard to overshoot with; a heavy blow frees
// more per swing but runs past the peak sooner -- the same speed/control trade the
// pan and the dig both have, here as light hammer vs. sledge.
static constexpr double TAP_BLOW    = 0.18;  // fraction crushed per light-hammer strike. AUTHORED.
static constexpr double SLEDGE_BLOW = 0.40;  // fraction per two-hand sledge strike. AUTHORED.

// A grain has to weigh something before the eye and the panel admit it exists.
static constexpr double SPECK = 1e-6;

// The copper minerals, the two that carry the metal through this slice. Used to
// read liberation and, at the end, what the pan won.
static bool is_copper_ore(int p) { return p == CUPRITE || p == CHALCOCITE; }
static bool is_sulfide(int p)    { return p == CHALCOCITE; }
static bool is_oxide_ore(int p)  { return p == CUPRITE || p == CASSITERITE; }

// ---------------------------------------------------------------------------
// Terminal. Lifted from dig.cpp (which lifted it from pan.cpp) -- a raw tty with a
// non-blocking read is the one piece of machinery every slice needs and none
// should reinvent.

static struct termios saved_tty;
static bool tty_raw = false;

static void restore_tty() {
    if (tty_raw) { tcsetattr(STDIN_FILENO, TCSANOW, &saved_tty); tty_raw = false; }
    std::printf("\033[?25h");
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
    std::printf("\033[?25l");
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
// What the eye can read off the heap. Two things, both real: how much of the ore
// is FREED from its rock (liberation), and how COARSE the pile still is (the grind
// profile). A person breaking rock can see both; neither is a projected pan
// result, on purpose.

// Liberation of the copper ore, summed over sizes: free grains / all ore.
static double copper_liberation(const Substance& s) {
    double freed = 0.0, total = 0.0;
    for (int p = 0; p < N_PHASE; ++p) {
        if (!is_copper_ore(p)) continue;
        for (int b = 0; b < N_SIZE; ++b) freed += s.freegrain[p][b];
        total += s.phase_mass(p);
    }
    return total > SPECK ? freed / total : 0.0;
}

// Mass in each size bin, coarse to fine, for the grind profile. Free and locked
// alike -- the eye sees a lump, not whether it is one mineral or two.
static void size_profile(const Substance& s, double out[N_SIZE]) {
    for (int b = 0; b < N_SIZE; ++b) {
        double m = 0.0;
        for (int p = 0; p < N_PHASE; ++p) m += s.freegrain[p][b] + s.composite[p][b];
        out[b] = m;
    }
}

static const int PANEL_COL = 44;
static const char* SIZE_ID[N_SIZE] = { "dust", "silt", "sand", "gravel" };

static void draw_panel(const Substance& pile) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    const double lib = copper_liberation(pile);
    line("what the heap looks like");
    line("%s", "------------------------------");

    // The liberation bar: how much ore is out of its rock. This is the honest
    // half of the readout -- it only ever climbs, and it is a thing you can see.
    line("ore freed from the rock:");
    char bar[21];
    const int fill = (int)std::lround(lib * 20.0);
    for (int i = 0; i < 20; ++i) bar[i] = i < fill ? '#' : '.';
    bar[20] = '\0';
    line("  [%s] %3.0f%%", bar, 100.0 * lib);
    line("%s", "");

    // The grind profile: where the mass sits, coarse to fine. As you hammer, it
    // walks left toward dust -- and dust is the thing a pan cannot hold. The panel
    // does not SAY that; it just shows the pile getting finer.
    double prof[N_SIZE];
    size_profile(pile, prof);
    double tot = 0.0; for (int b = 0; b < N_SIZE; ++b) tot += prof[b];
    line("how coarse the pile is:");
    for (int b = N_SIZE - 1; b >= 0; --b) {
        const double frac = tot > SPECK ? prof[b] / tot : 0.0;
        char pb[17];
        const int pf = (int)std::lround(frac * 16.0);
        for (int i = 0; i < 16; ++i) pb[i] = i < pf ? '=' : ' ';
        pb[16] = '\0';
        line("  %-6s |%s| %3.0f%%", SIZE_ID[b], pb, 100.0 * frac);
    }
    line("%s", "");
    line("total in the pile: %.0f g", tot * 1000.0);
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the hammer rings and rings; your ears have stopped hearing it",
    "chips of rock have cut the backs of your hands",
    "a fine grey dust hangs in the light and settles on everything",
    "the anvil-stone has a bright hollow beaten into it",
    "somewhere the water for the pan is running; you can hear it",
    "your shoulders have gone from aching to numb",
    "a shard flies off and stings your cheek",
    "the heap is smaller than it looks, and heavier",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(const Substance& pile, bool sledge, int blows, double t,
                 const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-46s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);

    std::printf("   the breaking stone, and the heap you hauled from the dig\n\n");
    std::printf("   in your hands: %s\n", sledge ? "a two-hand sledge" : "a one-hand hammer");
    std::printf("   blows struck:  %d\n\n", blows);

    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [space] strike   [t] hammer/sledge   [p] pan what you have"
                "   [q] give up\n");

    draw_panel(pile);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The result of committing the heap to the pan. Only what a scale and an eye at
// the pan's lip can tell you: how much FREE copper you won, at what grade, and --
// the thing you cannot see while you work -- how much of it went over the lip as
// dust because you hit it one too many times.
static void report_pan(const Substance& pile) {
    const SeparationResult r = separate(pile, PAN);

    double cu_feed = 0.0, cu_won = 0.0, cu_lost = 0.0;
    bool oxide = false, sulfide = false;
    for (int p = 0; p < N_PHASE; ++p) {
        if (!is_copper_ore(p)) continue;
        cu_feed += pile.phase_mass(p);
        for (int b = 0; b < N_SIZE; ++b) {
            cu_won  += r.concentrate.freegrain[p][b];   // free copper kept in the pan
            cu_lost += r.tailings.freegrain[p][b];      // free copper washed over the lip
        }
        if (pile.phase_mass(p) > SPECK) { if (is_oxide_ore(p)) oxide = true; if (is_sulfide(p)) sulfide = true; }
    }
    const double conc_mass = r.concentrate.total_mass();
    const double grade = conc_mass > SPECK ? cu_won / conc_mass : 0.0;
    const double recov = cu_feed  > SPECK ? cu_won / cu_feed  : 0.0;
    const double still_locked = cu_feed - cu_won - cu_lost;

    std::printf("   You carry the crushed rock to the water and pan it.\n\n");

    if (cu_won * 1000.0 < 0.05) {
        std::printf("   You win almost nothing. ");
        if (copper_liberation(pile) < 0.1)
            std::printf("The copper is still locked in the rock --\n"
                        "   the pan cannot part what the hammer never freed. You did not\n"
                        "   break it enough.\n\n");
        else
            std::printf("What little you freed you then beat to\n"
                        "   dust, and the dust went over the lip with the mud.\n\n");
        return;
    }

    std::printf("   You keep %.0f g of concentrate, and of it %.0f g is free copper\n",
                conc_mass * 1000.0, cu_won * 1000.0);
    std::printf("   ore -- a grade of %.0f%%. You recovered %.0f%% of the copper that\n",
                100.0 * grade, 100.0 * recov);
    std::printf("   was in the heap.\n\n");

    if (cu_lost * 1000.0 > 0.5)
        std::printf("   %.0f g of copper went over the lip as fines -- ground too small\n"
                    "   for the pan to hold. That is the price of the last few blows.\n\n",
                    cu_lost * 1000.0);
    if (still_locked * 1000.0 > 0.5)
        std::printf("   %.0f g is still locked in rock the pan set aside; it never came\n"
                    "   free of its gangue. More breaking would have freed it -- at a\n"
                    "   cost of grinding some of what you kept to dust.\n\n",
                    still_locked * 1000.0);
    if (oxide && sulfide)
        std::printf("   The concentrate mixes oxide and sulfide. They do not take the\n"
                    "   same fire: the oxide smelts straight, the sulfide must be roasted\n"
                    "   first. Two fires wait at the furnace.\n\n");
    std::printf("   Break too little and the pan wins nothing; break too much and it\n"
                "   washes away. Somewhere between was the most you could keep, and\n"
                "   nobody was going to tell you where.\n\n");
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "breaker: needs a terminal. Run it yourself: cd core && make breaker\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You are at the breaking stone with the heap you hauled up from the dig.\n\n"
        "   Every gram of it is rock. You can read the copper in it, but you cannot\n"
        "   smelt rock and you cannot pan rock -- the metal is locked inside, half\n"
        "   ore and half gangue, one grain. The hammer is the only thing that frees\n"
        "   it: each blow breaks the locked stone and drives the pile finer.\n\n"
        "   When you think you have freed enough, you take it to the water and pan\n"
        "   it, and the pan keeps the heavy copper and lets the light waste run off.\n\n"
        "   But the pan cannot hold dust. Beat the rock to powder and the copper you\n"
        "   freed will wash away with the mud. How hard to break it, and when to\n"
        "   stop, is the whole of it -- and nobody is going to tell you.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // The heap. A full-depth copper column at the hill center -- oxide cap and
    // sulfide root, mixed, all locked (geology.h). This is what dig.cpp hands you
    // when you shoulder the bag; here we materialize it the way pan.cpp
    // materializes its dirt, so the slice stands alone.
    Substance pile = dig_column(Place{0.0, 0.0}, 0.4);

    bool sledge = false;
    int blows = 0;
    double t = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "You set the first stone on the anvil-rock and raise the hammer.";
    double nag_t = 6;

    bool running = true, quit = false, panned = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 't') {
                sledge = !sledge;
                nag = sledge ? "You take up the two-hand sledge. It breaks more, and finer."
                             : "You go back to the one-hand hammer for a lighter blow.";
                nag_t = t + 3;
            }
            else if (c == ' ') {
                const double intensity = sledge ? SLEDGE_BLOW : TAP_BLOW;
                pile = crush(pile, intensity);
                ++blows;
                t += sledge ? 3.0 : 1.5;
                // Notice when the pile has gone mostly to dust -- an observable
                // fact about the heap, not a hint about what to do with it.
                double prof[N_SIZE]; size_profile(pile, prof);
                double tot = 0.0; for (int b = 0; b < N_SIZE; ++b) tot += prof[b];
                if (tot > SPECK && (prof[CLAY] + prof[SILT]) / tot > 0.6 && t > nag_t) {
                    nag = "Most of the heap is a fine grey powder now.";
                    nag_t = t + 4;
                }
            }
            else if (c == 'p') {
                if (pile.total_mass() < SPECK) {
                    nag = "There is nothing on the stone to pan.";
                    nag_t = t + 3;
                } else { running = false; panned = true; }
            }
        }

        t += 0.05;
        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 37) % N_AMBIENT]; ambient_t = t + 37; }
        if (t > nag_t) nag.clear();

        draw(pile, sledge, blows, t, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(0.05);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !panned) {
        std::printf("   You put down the hammer and leave the heap on the stone.\n\n");
        return 0;
    }

    report_pan(pile);
    std::printf("   It cost you %d minutes and %d seconds and %d blows.\n\n",
                (int)t / 60, (int)t % 60, blows);
    return 0;
}
