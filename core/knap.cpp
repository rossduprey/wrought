// Phase A, the eighth playable slice: knap a stone into an edge, and meet the two
// walls the whole tool staircase is built on.
//
// pan/dig/breaker/furnace/carry/char/gather each took one verb of the chain and asked
// whether it was a game. gather ended at a wall it could not cross -- hands win sticks
// and stone but never timber, because felling wants an axe, the axe wants a hafted
// edge, and the edge wants a KNAPPED STONE. This verb is that first edge: the one tool
// you can make with your hands and two rocks, and the rung below every pick a dig
// assumes. So this asks the question the whole staircase leans on -- can you make the
// first tool at all, and how good can the best of it ever be?
//
// It runs on knap.h -- Stone split by FRACTURE, the knap() blow with its two gates --
// and adds a streambed to search and a stone to strike in real time.
//
// -- The findings this slice is built on. ----------------------------------
//
// knap.h, and it teaches TWO. First, FRACTURE, NOT HARDNESS: an edge is a fresh
// fracture surface, and only a brittle, isotropic stone -- flint, chert, obsidian --
// breaks CONCHOIDALLY, in the smooth shells that leave an edge. The tough cobble your
// hands pocketed most easily (gather's "loose stone") is GRANULAR: struck, it powders.
// It is the HAMMER, never the blade. The core is a thing you must FIND, not trip over.
//
// Second, THE EDGE HAS A FLOOR. Knapping authors nothing -- the edge is not added, it
// is what remains when everything that is not the edge is struck away, and it conserves
// (flake + core + debris = the stone you started). And a stone edge is brittle, so it
// can only be so keen: keen enough to cut a haft, never a razor. That wall is the
// staircase itself -- the rung that beats it is not a better-knapped stone, it is metal
// (forge.h, Era 2). Effort does not reach the floor; the next material does.
//
// -- The gesture. ----------------------------------------------------------
//
// gather's honest gauge was the STAND -- a full bar of timber you could never move.
// char's was the fire's own SMOKE. This scene's gauge is the STONE ITSELF and the blow
// you just landed: you do not read a number that says "sharp," you read what came off.
// A flake that feathered clean is an edge; one that hinged off short is a stub; a core
// that burst is gone. And the one move that would seem to help -- strike harder, strike
// again to sharpen past the floor -- is the move that shatters what you had. Nobody
// tells you the tough cobble is the wrong stone, or that the edge tops out: you strike,
// and the stone teaches you.
//
// Build: make knap   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "knap.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// Streambed stock and the pace of the search. Authored game pacing -- the
// counterpart to gather's rake/pull rates -- setting how the hunt FEELS. No test or
// finding reads these. What they play against IS the finding: a conchoidal core is
// scarce and must be found; the tough cobble is everywhere and useless as a blade.
static constexpr double CORE_MASS      = 0.80;  // kg, a fist-sized flint nodule once you turn one up.
static constexpr double PROSPECT_RATE  = 0.055; // per second of searching -- a core turns up in ~18s of a full bed.
static constexpr double BED_FULL       = 3.0;   // "richness" of this stretch of streambed; thins as you pick it over.
static constexpr double BED_RATE       = 0.10;  // how fast searching picks the bed over.
static constexpr double PICKED_CLEAN   = 0.05;  // below this fraction of the bed, the likely cobbles are turned.
static constexpr double SPECK          = 1e-6;

// ---------------------------------------------------------------------------
// Terminal. The same raw tty every slice shares (pan -> ... -> gather); a
// non-blocking single-key read and nothing more.

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
// What the last blow did -- the honest gauge. You read the stone, not a score.
enum Blow { NO_BLOW, EDGE, HINGE, SHATTER, POWDER };

static const char* blow_face(Blow b) {
    switch (b) {
        case NO_BLOW:  return "";
        case EDGE:     return "a flake feathers off clean -- one edge, keen where the two scars meet";
        case HINGE:    return "the flake hinges off short and blunt -- a stub, no edge, mass gone for nothing";
        case SHATTER:  return "the core bursts in your hand -- a scatter of debris and nothing to keep";
        case POWDER:   return "it only powders under the blow -- this stone will not flake, it is the wrong rock";
    }
    return "";
}

static const int PANEL_COL = 46;

static void draw_panel(bool have_core, const Stone& core, int edges, double edge_mass,
                       double debris, Blow last, double bed_left) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("the stone in your hands");
    line("%s", "------------------------------");

    auto bar16 = [&](double frac, char full_ch) {
        static char b[17];
        const int pf = (int)std::lround((frac < 0 ? 0 : frac > 1 ? 1 : frac) * 16.0);
        for (int i = 0; i < 16; ++i) b[i] = i < pf ? full_ch : ' ';
        b[16] = '\0';
        return b;
    };

    // The two stones, named for the roles knap.h gives them.
    line("  hammerstone     |%s| tough cobble -- survives the blow", bar16(1.0, '#'));
    if (have_core)
        line("  flint core      |%s| %4.0f g -- breaks the RIGHT way", bar16(core.mass / CORE_MASS, '='), core.mass * 1000.0);
    else
        line("  flint core      |%s| none yet -- search the bed for one", bar16(0.0, '='));
    line("%s", "");

    // What the last blow did, in the stone's own terms.
    line("%s", last == NO_BLOW ? "you have not struck yet" : blow_face(last));
    line("%s", "");

    // What you have knapped, and the wall it stops at.
    line("  edges won        %d   (%.0f g of flakes)", edges, edge_mass * 1000.0);
    line("  debris           %.0f g -- struck away, never an edge", debris * 1000.0);
    if (edges > 0)
        line("  edge angle      ~%.0f deg -- the stone's floor; a razor wants metal", STONE_EDGE_FLOOR);
    line("%s", "");

    if (!have_core) {
        line("  the bed         |%s| %s", bar16(bed_left / BED_FULL, '.'),
             bed_left / BED_FULL < PICKED_CLEAN ? "picked over -- move on" : "cobbles to turn");
    }
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the stream turns the cobbles over for you, endlessly, and none of them ring right",
    "your thumb finds the old scar on a flint where it last broke -- that is the one",
    "a struck flake catches the light along an edge thinner than anything you own",
    "the tough grey cobbles are everywhere; the stone that flakes is one in a hundred",
    "you weigh a stone in the palm and know before you strike it that it will only crumble",
    "the pile of debris at your feet is most of what a good core was",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(bool have_core, const Stone& core, int edges, double edge_mass,
                 double debris, Blow last, double bed_left, bool searching,
                 double t, const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);
    std::printf("   the streambed -- a tough cobble for a hammer, and a hunt for a core\n\n");
    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [f] search the bed for a core%s\n", searching ? "  (searching...)" : "");
    std::printf("   strike:  [a] a careful edge-on tap   [x] a square blow   [h] a hard blow\n");
    std::printf("   [r] pocket your flakes and go   [q] walk away\n");
    draw_panel(have_core, core, edges, edge_mass, debris, last, bed_left);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The reckoning. What a scale and an eye tell you at the end: what you struck off,
// whether any of it will cut, the mass ledger that proves the edge was subtracted,
// and the two walls the scene exists to teach. The findings lean on knap.h's gates,
// not on numbers restated here.
static void report_knap(int edges, double edge_mass, double debris,
                        double core_left, bool ever_had_core) {
    std::printf("   You sit back and take stock of the stone you worked.\n\n");

    if (!ever_had_core) {
        std::printf("   You never turned up a core. All you had was the tough cobble your hands\n"
                    "   pocketed most easily -- and struck, it only powders. The stone that comes\n"
                    "   free is the HAMMER; the stone that takes an edge you have to hunt for, and\n"
                    "   you did not find it. Fracture, not hardness, is the whole gate.\n\n");
        return;
    }

    if (edges == 0) {
        std::printf("   You found a core that would flake -- and struck no edge from it. Every\n"
                    "   blow hinged short or burst it: the stone was willing, the hand was not.\n"
                    "   %.0f g of it is debris now. Knapping takes away; it never gives back.\n\n",
                    debris * 1000.0);
        return;
    }

    std::printf("   %d flake%s off a flint core, %.0f g of edge -- keen where two fresh scars\n"
                "   meet, and keen enough to cut a haft or skin a kill. You struck %.0f g away\n"
                "   as debris getting there; the core that is left weighs %.0f g. Add them and\n"
                "   you have the stone you started with: the edge was never added, only found.\n\n",
                edges, edges == 1 ? "" : "s", edge_mass * 1000.0, debris * 1000.0, core_left * 1000.0);

    // The floor -- the sentence the scene exists to teach, and it comes only now.
    std::printf("   And this is as keen as stone gets: about %.0f degrees at the bit, blunt\n"
                "   enough to survive the cut. Strike to sharpen it past that and it chips or\n"
                "   shatters -- the floor is a wall no skill gets under. The tool that beats it\n"
                "   is not a better stone; it is metal, and metal is a furnace and an anvil away.\n\n",
                STONE_EDGE_FLOOR);

    // The staircase, one rung up.
    std::printf("   But the edge you hold is enough to shape a haft -- and a haft plus this\n"
                "   edge is the axe that fells the timber your hands could not, and the pick\n"
                "   that wins the hard rock a dig only scratched. Tool number one has made\n"
                "   tool number two. The staircase turns.\n\n");
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "knap: needs a terminal. Run it yourself: cd core && make knap\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   gather left you at a wall: your hands win sticks and loose stone, but never\n"
        "   the timber a haft is cut from, because felling wants an axe -- and the axe\n"
        "   wants an edge, and the first edge is not metal. It is a knapped stone.\n\n"
        "   You have two rocks. One is the tough grey cobble your hands pocketed by the\n"
        "   armful: hard, blunt, and it survives a blow -- a HAMMER. The other you do not\n"
        "   have yet. An edge is a fresh break, and only a brittle, glassy stone -- flint,\n"
        "   chert -- breaks in the smooth shells that leave one. That stone is scarce, and\n"
        "   it is in the streambed if it is anywhere. Find it, then strike it.\n\n"
        "   How you strike is the whole craft: a careful blow proud of an edge peels a\n"
        "   flake clean; a square blow sinks in and hinges off a useless stub; a hard\n"
        "   blow bursts the core. And however well you strike, a stone edge is only ever\n"
        "   so keen. You will feel where it stops.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    const Stone hammer { 1.0, GRANULAR };   // the tough cobble gather let you carry off.
    Stone core { 0.0, CONCHOIDAL };
    bool have_core = false, ever_had_core = false;
    int edges = 0;
    double edge_mass = 0.0, debris = 0.0;
    double bed_left = BED_FULL, prospect = 0.0;
    bool searching = false;
    Blow last = NO_BLOW;
    double t = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "First find a core: press [f] to search the bed. The cobble in your hand is only the hammer.";
    double nag_t = 10;

    // Strike the current core one blow, the given way. Enforces knap.h's gates: with
    // no core you are striking the tough cobble AS a core, and it powders.
    auto strike = [&](Strike how) {
        const Stone& c = have_core ? core : hammer;   // no core -> you strike the cobble
        const KnapResult r = knap(c, hammer, how);
        if (!have_core) { last = POWDER;              // granular core: knap() gives only grit
            nag = "Granular. It powders. The stone you carried is the hammer, not the blade -- find a core.";
            nag_t = t + 7; return; }
        if (r.flake.usable) { edges += 1; edge_mass += r.flake.mass; last = EDGE;
            if (edges == 1) { nag = "An edge -- keener than any metal you own, because you own none. That is tool #1's tool.";
                              nag_t = t + 7; } }
        else if (how == STRIKE_STEEP) { edge_mass += 0.0; last = HINGE; }
        else { debris += r.flake.mass > 0.0 ? r.flake.mass : (c.mass - r.core.mass); last = SHATTER;
               nag = "You struck to force it and it burst. Stone has a floor; past it, it shatters -- that is the wall.";
               nag_t = t + 7; }
        // Steep flakes also leave mass -- count it as debris (a stub, not an edge).
        if (how == STRIKE_STEEP) debris += r.flake.mass;
        core = r.core;
        if (core.mass < SPECK) { have_core = false; core.mass = 0.0;
            nag = "The core is worked down to nothing. Search the bed for another if you want more.";
            nag_t = t + 6; }
    };

    bool running = true, quit = false, kept = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'r') { running = false; kept = true; }
            else if (c == 'f') { searching = true; }
            else if (c == 'a') { searching = false; strike(STRIKE_ACUTE); }
            else if (c == 'x') { searching = false; strike(STRIKE_STEEP); }
            else if (c == 'h') { searching = false; strike(STRIKE_HARD);  }
        }

        const double dt = 0.05;
        t += dt;

        // Searching turns the bed over. A core turns up when prospect fills -- rarer as
        // the bed is picked over, and nil once it is turned. You do not stockpile cores;
        // you find one, work it, and find the next. The tough cobble is never scarce.
        if (searching && !have_core) {
            const double frac = bed_left / BED_FULL;
            if (frac > PICKED_CLEAN) {
                prospect += PROSPECT_RATE * frac * dt;
                bed_left -= BED_RATE * frac * dt;
                if (prospect >= 1.0) {
                    core = Stone{ CORE_MASS, CONCHOIDAL };
                    have_core = true; ever_had_core = true;
                    prospect = 0.0; searching = false; last = NO_BLOW;
                    nag = "A core -- it rings right and shows a glassy scar. Now strike: [a] careful, [x] square, [h] hard.";
                    nag_t = t + 8;
                }
            } else {
                searching = false;
                nag = "This stretch is picked over -- every likely cobble turned, and no flint. Nothing more here.";
                nag_t = t + 7;
            }
        }

        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 41) % N_AMBIENT]; ambient_t = t + 41; }
        if (t > nag_t) nag.clear();

        draw(have_core, core, edges, edge_mass, debris, last, bed_left, searching,
             t, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(dt);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !kept) {
        std::printf("   You drop the stones back in the water and straighten up.\n\n");
        return 0;
    }

    report_knap(edges, edge_mass, debris, core.mass, ever_had_core);
    std::printf("   It cost you %d minutes and %d seconds at the water.\n\n",
                (int)t / 60, (int)t % 60);
    return 0;
}
