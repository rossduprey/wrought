// Phase A, the seventh playable slice: gather wood by hand, and meet the wall.
//
// pan/dig/breaker/furnace/carry/char each took one verb of the chain and asked
// whether it was a game. char.cpp fed the furnace by MAKING its charcoal instead
// of spawning it -- but it started from a heap of wood banked by fiat. This verb
// removes that fiat one layer further back: before the pit there is the forest,
// and before the axe there are only your hands. So this asks the verb every earlier
// slice quietly assumed: where does the wood come from, and what can hands alone win?
//
// It runs on fuel.h's Wood -- tinder, sticks, timber -- and adds nothing but a
// clearing that depletes as you work it, and the one thing hands cannot do.
//
// -- The finding this slice is built on. -----------------------------------
//
// DESIGN.md, the tool bootstrap: a pick or axe is a head plus a haft, and a haft is
// worked TIMBER -- but felling timber needs the axe, which needs a hafted edge, which
// needs worked wood and a knapped stone. Hands are tool #1: the WORST point on the
// curve, not "no tool." They rake dead needles for tinder and pull dead sticks from a
// standing tree, and there they stop. The timber -- the bulk fuel a pit really wants,
// and the only stock a haft is ever cut from -- stays a standing trunk. This is the
// circle the model long hand-waved: the pick every dig assumes and the charcoal every
// furnace burned both needed a tool that needed wood that needed a tool.
//
//   THE FOREST IS FULL OF WOOD AND HANDS REACH ALMOST NONE OF IT.
//
// fuel.h already carries the gate, and this slice only makes you feel it: Wood.burnable()
// counts sticks and timber but not tinder (the match is not the fuel); can_light() needs
// dry tinder; pyrolyze() turns burnable wood to carbon. Gather all tinder and the pile
// lights and chars nothing; gather all sticks and it never takes a spark; and the timber
// column -- half of the pile char.cpp banked -- you cannot add to at all.
//
// -- The gesture. -----------------------------------------------------------
//
// The furnace read its fire by colour, the pit by its smoke. This scene's honest gauge
// is the STAND: a full, static bar of standing timber you can never move, beside the
// dead wood within reach that thins as you strip it. You choose where to spend your
// hands -- rake, pull, or pocket stone for the other road -- and the one key that would
// change everything, [f] to fell, is the one that does nothing. What you shoulder and
// carry off is what has already fallen. Nobody tells you the trunk is off limits: you
// lean on it and learn.
//
// Build: make gather   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "char.h"   // pulls in fuel.h: Wood, can_light, pyrolyze

using namespace wrought;

// ---------------------------------------------------------------------------
// The clearing's stock and the pace of hands. Authored game pacing -- the
// counterpart to char.cpp's burn rates -- setting how the work FEELS. No test or
// finding reads these. What they play against IS the finding: fuel.h's Wood splits
// wood by HOW it is won, and hands win only two of its three columns. These numbers
// only decide how long the reachable share takes, never that the timber is locked.
static constexpr double DEADWOOD_FULL = 3.0;   // kg of dead sticks within a hand's reach in this stand.
static constexpr double LITTER_FULL   = 0.5;   // kg of dry needle litter on the ground under it.
static constexpr double STONE_FULL    = 4.0;   // kg of loose stone scattered about -- not the scarce thing.

static constexpr double PULL_RATE  = 0.045;    // kg/s of sticks at a full stand -- a dead branch every few seconds.
static constexpr double RAKE_RATE  = 0.030;    // kg/s of tinder raked from fresh litter.
static constexpr double STONE_RATE = 0.060;    // kg/s of stone -- it is only lying there.

static constexpr double GATHER_MOISTURE = 0.15; // dead standing wood is dry-ish; that is why you take the DEAD stuff.
static constexpr double PICKED_CLEAN = 0.05;    // below this fraction of a source, the easy pickings are gone.
static constexpr double SPECK = 1e-6;

// ---------------------------------------------------------------------------
// Terminal. The same raw tty every slice shares (pan -> dig -> ... -> char); a
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
// What your hands are set to. IDLE first: the first decision is yours.
enum Doing { IDLE, RAKE, PULL, STONE };

// What the eye reads off the stand: how the current work is going, and whether the
// easy share is spent. Like the furnace's colour and the pit's smoke, this is the
// honest gauge -- no kilograms of "how much is left" on the panel, only how the reach
// feels as it thins.
static const char* hands_face(Doing doing, double litter_left, double deadwood_left,
                              double stone_left) {
    switch (doing) {
        case IDLE:
            return "your hands are empty; you look over the stand and choose";
        case RAKE:
            return litter_left / LITTER_FULL < PICKED_CLEAN
                     ? "you scrape at bare ground -- the dry litter here is raked up"
                     : "you rake dry needles into a heap and pocket the fine tinder";
        case PULL:
            return deadwood_left / DEADWOOD_FULL < PICKED_CLEAN
                     ? "the easy deadwood is stripped; what is left is green or too high"
                     : "you snap dead branches from the standing trunks, an armful at a time";
        case STONE:
            return stone_left / STONE_FULL < PICKED_CLEAN
                     ? "you have picked the loose stone from around your feet"
                     : "you turn over loose stone and pocket the pieces that fit the hand";
    }
    return "";
}

static const int PANEL_COL = 46;

static void draw_panel(double tinder_got, double sticks_got, double stone_got,
                       double litter_left, double deadwood_left, double stone_left,
                       Doing doing) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("the stand");
    line("%s", "------------------------------");
    line("%s", hands_face(doing, litter_left, deadwood_left, stone_left));
    line("%s", "");

    // The honest gauge: the standing timber, a full bar that never moves, over the dead
    // wood within reach that you watch thin. The gap between them is the whole finding.
    auto bar16 = [&](double frac, char full_ch) {
        static char b[17];
        const int pf = (int)std::lround((frac < 0 ? 0 : frac > 1 ? 1 : frac) * 16.0);
        for (int i = 0; i < 16; ++i) b[i] = i < pf ? full_ch : ' ';
        b[16] = '\0';
        return b;
    };
    line("  standing timber |%s| needs an axe", bar16(1.0, '#'));
    line("  deadwood, reach |%s| by hand", bar16(deadwood_left / DEADWOOD_FULL, '='));
    line("%s", "");

    // What you have shouldered so far, each against the reachable share of its own
    // source -- so a full bar means you have taken all your hands can, not "enough."
    auto matbar = [&](const char* name, double m, double full) {
        const double frac = full > SPECK ? m / full : 0.0;
        static char b[17];
        const int pf = (int)std::lround((frac < 0 ? 0 : frac > 1 ? 1 : frac) * 16.0);
        for (int i = 0; i < 16; ++i) b[i] = i < pf ? '=' : ' ';
        b[16] = '\0';
        line("  %-13s |%s| %5.0f g", name, b, m * 1000.0);
    };
    matbar("tinder",  tinder_got, LITTER_FULL);
    matbar("sticks",  sticks_got, DEADWOOD_FULL);
    matbar("stone",   stone_got,  STONE_FULL);
    line("%s", "");

    const char* what =
        doing == RAKE  ? "raking tinder -- the match, not the fuel"
      : doing == PULL  ? "pulling sticks -- what a fire actually burns"
      : doing == STONE ? "gathering stone -- the seed of the other road"
                       : "hands idle -- pick where to spend them";
    line("you are %s", what);
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the canopy moves and lets down a shifting coin of light onto the litter",
    "somewhere a dead limb lets go and comes down through the branches",
    "your back tells you how long you have been bent to the ground",
    "the standing trunks go up past reach, straight and full of wood you cannot cut",
    "a squirrel scolds you from a fork and is gone",
    "the pile at your feet looks smaller than the work that made it",
    "green wood bends and will not snap; only the dead stuff comes free",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(double tinder_got, double sticks_got, double stone_got,
                 double litter_left, double deadwood_left, double stone_left,
                 Doing doing, double t, const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);
    std::printf("   the clearing -- your hands are the only tool you have brought\n\n");
    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [t] rake tinder   [s] pull sticks   [k] pick up stone\n");
    std::printf("   [f] fell a tree   [r] shoulder it and go   [q] walk away\n");
    draw_panel(tinder_got, sticks_got, stone_got, litter_left, deadwood_left, stone_left, doing);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The reckoning. Only what a scale and an eye can tell you at the end of a day's
// gathering: what you shouldered, whether it will light, what it makes as char, and
// the wall you kept meeting. The finding is stated after, as consequence -- and it
// leans on fuel.h's own gates, not on numbers restated here.
static void report_gather(double tinder_got, double sticks_got, double stone_got) {
    Wood got;
    got.tinder   = tinder_got;
    got.sticks   = sticks_got;
    got.timber   = 0.0;              // the column hands cannot fill.
    got.moisture = GATHER_MOISTURE;

    std::printf("   You shoulder the load and take stock of a day's gathering.\n\n");
    std::printf("   %.0f g of tinder, %.0f g of sticks", tinder_got * 1000.0, sticks_got * 1000.0);
    if (stone_got * 1000.0 >= 1.0) std::printf(", and %.0f g of loose stone", stone_got * 1000.0);
    std::printf(".\n\n");

    // The light gate and the char, read straight off fuel.h -- not asserted here, just
    // reported. Tinder is the match; sticks are the fuel; timber would have been both.
    const double char_kg = pyrolyze(got).phase_mass(CARBON);
    if (!can_light(got)) {
        std::printf("   Not a wisp of dry tinder in it. Sticks and stone, and no way to\n"
                    "   take a spark -- this pile will not light. The match is the thing\n"
                    "   you did not stoop for.\n\n");
    } else if (got.burnable() < SPECK) {
        std::printf("   It will take a spark -- but there is nothing here to sustain a fire,\n"
                    "   only tinder, which flares and is gone. Tinder starts a burn; it is\n"
                    "   not the burn.\n\n");
    } else {
        std::printf("   Dry enough to light, and run through the pit it would give about\n"
                    "   %.0f g of charcoal. char.cpp banked its pit with six hundred grams\n"
                    "   of wood a firing; your hands, a day at it, brought a share of one.\n\n",
                    char_kg * 1000.0);
    }

    // The wall. This is the sentence the scene exists to teach, and it comes only now.
    std::printf("   And every stick of it had already fallen. The standing timber -- the\n"
                "   bulk of the wood, and the only stock a haft is ever cut from -- you\n"
                "   could not touch, because felling it wants an axe, and the axe wants a\n"
                "   hafted edge, and the edge wants a knapped stone.\n\n");
    if (stone_got * 1000.0 >= 1.0) {
        std::printf("   The stone in your pouch is the first step of that road: the hammerstone\n"
                    "   and the edge that, eventually, fells the very trunk you leaned on.\n\n");
    } else {
        std::printf("   You pocketed no stone, and so began nothing of the road that ends in\n"
                    "   the axe. Hands are tool number one -- the worst point on the curve.\n\n");
    }
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "gather: needs a terminal. Run it yourself: cd core && make gather\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   Before the pit there is the forest, and before the axe there are only\n"
        "   your hands. The furnace ate charcoal; the pit ate wood; and the wood has\n"
        "   to come from somewhere. It comes from here.\n\n"
        "   You can rake the dry needle litter into tinder -- the match a fire needs\n"
        "   to catch. You can pull the dead branches off the standing trunks -- the\n"
        "   sticks a fire actually burns. And you can pocket the loose stone underfoot,\n"
        "   for a road that does not start with wood at all.\n\n"
        "   What you cannot do is fell a tree. The timber -- straight, dense, the bulk\n"
        "   of every trunk in view, and the only wood a tool handle is ever cut from --\n"
        "   stays standing. Your hands will not bring it down, and nothing you brought\n"
        "   today will either. You take what has already fallen, and no more.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    double tinder_got = 0.0, sticks_got = 0.0, stone_got = 0.0;
    double litter_left   = LITTER_FULL;
    double deadwood_left = DEADWOOD_FULL;
    double stone_left    = STONE_FULL;
    Doing doing = IDLE;
    double t = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "Pick where to spend your hands: [t] tinder, [s] sticks, [k] stone.";
    double nag_t = 8;

    bool running = true, quit = false, shouldered = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'r') { running = false; shouldered = true; }
            else if (c == 't') { doing = RAKE;  }
            else if (c == 's') { doing = PULL;  }
            else if (c == 'k') { doing = STONE; }
            else if (c == 'f') {
                // The one key that changes nothing. This is the teaching beat.
                nag = "You set your hands to a standing trunk. Without an edge to bite it, "
                      "it stays a tree.";
                nag_t = t + 6;
            }
        }

        const double dt = 0.05;
        t += dt;

        // Hands win mass at a rate that tapers as the reachable share of a source runs
        // out -- the last dead stick is always just out of reach. Below PICKED_CLEAN the
        // easy pickings are spent and the rate is nil: you do not empty a stand, you
        // exhaust what hands can take from it. The timber never enters this arithmetic.
        auto work = [&](double& got, double& left, double full, double rate) {
            const double frac = left / full;
            if (frac <= PICKED_CLEAN) return;
            const double d = rate * frac * dt;
            const double take = d < left ? d : left;
            got  += take;
            left -= take;
        };
        if (doing == RAKE)  work(tinder_got, litter_left,   LITTER_FULL,   RAKE_RATE);
        else if (doing == PULL)  work(sticks_got, deadwood_left, DEADWOOD_FULL, PULL_RATE);
        else if (doing == STONE) work(stone_got,  stone_left,    STONE_FULL,    STONE_RATE);

        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 37) % N_AMBIENT]; ambient_t = t + 37; }
        if (t > nag_t) nag.clear();

        draw(tinder_got, sticks_got, stone_got, litter_left, deadwood_left, stone_left,
             doing, t, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(dt);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !shouldered) {
        std::printf("   You straighten up, leave the half-made pile on the ground, and go.\n\n");
        return 0;
    }

    report_gather(tinder_got, sticks_got, stone_got);
    std::printf("   It cost you %d minutes and %d seconds in the clearing.\n\n",
                (int)t / 60, (int)t % 60);
    return 0;
}
