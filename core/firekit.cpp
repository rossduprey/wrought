// Phase A, the tenth playable slice: make the spark, build the kit, and strike it.
//
// gather.cpp won you a satchel of tinder and sticks; char.cpp banked a pile and lit
// it -- "you touch a brand to the pile and it catches." But where did the BRAND come
// from? Every earlier fire in this project lit itself the instant the tinder was dry:
// fuel.h's can_light() returns true on dry tinder alone, as if a flame were lying in
// the grass waiting. It is not. This slice is the verb that free spark hid: before any
// fire there is the MAKING of fire, and the first tool a person ever makes is the one
// that makes the rest possible.
//
// It runs on firekit.h -- fire_tier() for the ladder, make_smolder()/ignite() for the
// spark -- and adds nothing but a cold station you load rung by rung, a friction set
// you carve once, and the one strike that tells you what you built.
//
// -- The finding this slice is built on. -----------------------------------
//
// firekit.h, two structural facts and no number for either:
//
//   THE FUEL DOES NOTHING WITHOUT THE PLAYER, AND THE PLAYER DOES NOTHING WITHOUT
//   THE SPARK.
//
//   1. THE IGNITION LADDER. No spark catches a log. A spark catches tinder, tinder
//      lights sticks, sticks light split timber -- each rung lights the next, so a
//      fire is only ever as high as its lowest MISSING rung. Timber laid with no
//      tinder under it catches nothing. And the top rung, TIMBER, is the one your
//      hands could not gather (gather.cpp: the axe is not made) -- so the bulk fire a
//      pit or a bloom really wants is LOCKED here, exactly as the pick's reach was
//      locked behind the whole staircase. A hand-gathered kit tops out at a stick
//      fire: enough to carry a copper reduction, never a smelt-grade blast.
//
//   2. THE SPARK IS A TOOL, NOT A FUEL. The smolder set is a friction drill carved
//      once, from DRY dead wood -- a green spindle only polishes and never smokes --
//      and then kept. It does not wear; the firemaking GESTURE (the bow, the coal,
//      the tinder nest blown to flame) is time-compressed off-screen, the same gloss
//      as the panner's wrist and the axe swing. Possession is the whole of it. A
//      station heaped with a perfect kit is INERT until you strike it with the tool.
//
// -- The gesture. -----------------------------------------------------------
//
// The furnace read its fire by colour, the pit by its smoke, the clearing by its
// thinning reach. This station's honest gauge is the LADDER: the rungs you have laid,
// bottom to top, and a spark you have carved or have not. Nothing shows you what will
// catch -- you learn that only when you STRIKE, the way the pit tells its yield only
// when you rake it out. Strike with no smolder set and the pile just sits there (where
// is the spark?); strike a tinder nest with nothing above it and it flares and is gone;
// reach for the timber rung and your hand closes on air. Nobody says any of this. You
// strike, and the cold pile teaches you.
//
// Build: make firekit   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "firekit.h"   // pulls in fuel.h: Wood, TINDER_MOISTURE_MAX, fire_tier, ignite

using namespace wrought;

// ---------------------------------------------------------------------------
// The station's pacing. Authored game feel -- the counterpart to char.cpp's burn
// rates -- and no test or finding reads any of it. The rung LOADS are presence
// markers (fire_tier reads > 0, not how much); the moistures only land on either
// side of fuel.h's TINDER_MOISTURE_MAX (issue #32), the one real gate here, so a dry
// stick catches and a green one does not. FLARE_LIFE is how long a tinder-only fire
// gets to disappoint you before it dies. Slice-local pacing, not a finding.
static constexpr double TINDER_KIT = 0.03;   // kg, a handful of nest -- the match, not the fuel.
static constexpr double STICKS_KIT = 0.50;   // kg, an armful of dead sticks laid over the nest.

static constexpr double DRY  = 0.12;   // dry pouch tinder / dead stick: below TINDER_MOISTURE_MAX, it catches.
static constexpr double DAMP = 0.55;   // a handful off the ground / a green shoot: above it, nothing takes.

static constexpr double FLARE_LIFE = 3.0;    // s a tinder flare burns before it goes to nothing.

// ---------------------------------------------------------------------------
// Terminal. The same raw tty every slice shares (pan -> dig -> ... -> gather); a
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
// The station's three honest states, straight off firekit.h. Nothing here burns on
// its own: a loaded station is INERT storage until the player strikes it. FLARING is
// LIT that could not hold -- a tinder-only catch on its way to nothing.
enum Stage { COLD, FLARING, CAUGHT };

// What tinder is in the nest right now. NONE first: the bottom rung is a decision.
enum TinderState { T_NONE, T_DRY, T_DAMP };

// What your hand is on for the spark's spindle. A green shoot polishes and never
// smokes; only the dry dead stick takes an ember -- fuel.h's dryness gate, on the
// TOOL now, not the fuel.
enum SparkStock { STOCK_GREEN, STOCK_DRY };

// ---------------------------------------------------------------------------
// Build the live FireKit from what is laid in the station. Presence, not amount:
// fire_tier() only asks whether each rung is there and whether the bottom one is dry.
static FireKit current_kit(TinderState tin, bool sticks_laid) {
    FireKit k;
    k.tinder   = (tin == T_NONE) ? 0.0 : TINDER_KIT;
    k.sticks   = sticks_laid ? STICKS_KIT : 0.0;
    k.timber   = 0.0;                       // the locked rung: hands never gathered it.
    k.moisture = (tin == T_DRY) ? DRY : DAMP;
    return k;
}

// ---------------------------------------------------------------------------
// What the eye reads off the station: the fire, or the cold pile. Like the furnace's
// colour and the pit's smoke, this is the honest gauge -- no tier name on the panel
// until you have struck it and are watching the thing itself.
static const char* fire_face(Stage stage, FireTier caught_tier) {
    switch (stage) {
        case COLD:
            return "the laid kit sits cold in the ring of stones, waiting on you";
        case FLARING:
            return "the nest flares up bright -- and finds nothing above it to take";
        case CAUGHT:
            return caught_tier >= TIMBER_FIRE
                     ? "a deep fire stands in the ring, timber and all alight"
                     : "flame climbs off the nest into the sticks and holds -- a steady fire";
    }
    return "";
}

static const int PANEL_COL = 46;

static void draw_panel(TinderState tin, bool sticks_laid, bool have_smolder,
                       SparkStock stock, Stage stage, FireTier caught_tier) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("the fire station");
    line("%s", "------------------------------");
    line("%s", fire_face(stage, caught_tier));
    line("%s", "");

    // The ladder, drawn top-down as you SEE it stacked, but read bottom-up: a rung
    // catches only if everything under it caught. The timber rung is a wall -- it is
    // never yours to lay here, because felling timber wants the axe (gather.cpp).
    line("  the ladder (each rung lights the one above)");
    line("    timber   [ %-8s ]  needs the axe", "locked");
    line("    sticks   [ %-8s ]  the fire's fuel", sticks_laid ? "laid" : "----");
    const char* tin_s = tin == T_DRY  ? "dry"
                      : tin == T_DAMP ? "damp"
                                      : "----";
    line("    tinder   [ %-8s ]  the match, not fuel", tin_s);
    line("%s", "");

    // The spark: a made tool, kept. Possession is the whole of it -- there is no
    // wear bar, because there is no wear. Below it, the stock your hand is on.
    line("  the spark   [ %-8s ]  %s",
         have_smolder ? "carved" : "--",
         have_smolder ? "a friction set, made once and kept"
                      : "no fire starts without it");
    line("  stock at hand: %s",
         stock == STOCK_DRY ? "a dry dead stick -- it will take an ember"
                            : "a green shoot -- it only polishes, never smokes");
    line("%s", "");

    const char* what =
        stage == CAUGHT  ? "it is lit -- carry the fire off, or let it burn"
      : stage == FLARING ? "too little under the flame -- it is going to nothing"
      : !have_smolder    ? "no spark yet: carve the smolder set before you strike"
                         : "loaded -- but nothing burns until you STRIKE it";
    line("%s", what);
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the cold stones of the ring hold last night's ash and nothing warm",
    "you turn a dead stick in your hands, feeling for the dry heartwood",
    "wind moves through the clearing and you cup your body around the kit",
    "the tinder nest is fine as birdsdown and just as quick to waste",
    "somewhere the memory of the first person who ever did this on purpose",
    "the green shoots bend and weep; the grey dead wood snaps clean and dry",
    "a kit is only kindling until the one thing that lights it is in your hand",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(TinderState tin, bool sticks_laid, bool have_smolder, SparkStock stock,
                 Stage stage, FireTier caught_tier, double t, const char* ambient,
                 const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);
    std::printf("   the fire station -- a ring of stones, and everything you gathered\n\n");
    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [d] hand on green/dry stock   [c] carve the smolder set (the spark)\n");
    std::printf("   [t] tinder: none/dry/damp     [s] lay / clear the sticks\n");
    std::printf("   [k] lay timber   [x] STRIKE   [g] carry the fire off   [q] walk away\n");
    draw_panel(tin, sticks_laid, have_smolder, stock, stage, caught_tier);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The reckoning. Only what you know standing over the fire you made -- or the cold
// pile you did not. The finding is stated after, as consequence, and leans on
// firekit.h's own ladder, not on numbers restated here.
static void report_fire(FireTier tier) {
    if (tier == NO_FIRE) {
        std::printf("   You never got a fire. Somewhere in it a rung was missing -- no spark\n"
                    "   in your hand, or no dry tinder to take one -- and a fire is only ever\n"
                    "   as high as its lowest missing rung. The cold pile taught you which.\n\n");
        return;
    }

    std::printf("   You carry the fire off, cupped and fed, to whatever wants it.\n\n");

    if (tier == TINDER_FLARE) {
        std::printf("   Though it is barely a fire -- a tinder flare, bright and brief. It\n"
                    "   will light the next thing you hold to it, but it is not a burn you\n"
                    "   can smelt or char over. Tinder starts a fire; it is not one.\n\n");
    } else if (tier == STICK_FIRE) {
        std::printf("   A stick fire -- a real, sustained flame off dead hand-gathered wood.\n"
                    "   It will carry a copper reduction and warm a camp. But look at the\n"
                    "   ladder: the top rung, TIMBER, you never laid, because your hands\n"
                    "   never felled any. This is the highest fire hands alone can build.\n\n");
    } else { // TIMBER_FIRE -- unreachable by hand here; kept honest for when the axe exists.
        std::printf("   A timber fire -- the deep, bulk blast a pit or a bloom actually wants.\n"
                    "   You could only lay that top rung because the axe exists to fell it.\n\n");
    }

    // The wall, and the whole point: the spark was the first tool, and the ladder is
    // why the staircase runs the way it does. Stated only now, as consequence.
    std::printf("   And none of it lit itself. The spark was a set you carved from a dead\n"
                "   dry stick -- the first tool a person ever makes, the one that makes\n"
                "   every fire after it possible. The fuel did nothing until you brought\n"
                "   it the one thing the clearing never handed you: the fire to start it.\n\n");
    if (tier == STICK_FIRE) {
        std::printf("   The smelt-grade fire waits on the axe, same as everything else: the\n"
                    "   timber rung is the pick's reach in another shape -- locked, until the\n"
                    "   tool that frees it is made.\n\n");
    }
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "firekit: needs a terminal. Run it yourself: cd core && make firekit\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You have carried the day's gathering to a ring of cold stones: a nest of\n"
        "   fine tinder, an armful of dead sticks. Every scene before this one lit its\n"
        "   fire the moment the tinder was dry, as if a flame were lying in the grass.\n"
        "   It is not. A fire has to be MADE, and making it is a tool before it is a\n"
        "   heap.\n\n"
        "   First carve the spark: a friction set, worked from a dry dead stick -- a\n"
        "   green one only polishes and never smokes. Made once, it is yours to keep;\n"
        "   it does not wear. Then build the ladder from the bottom: a spark catches\n"
        "   tinder, tinder lights sticks, sticks light timber -- each rung lights the\n"
        "   one above, so a gap anywhere stops the climb.\n\n"
        "   Lay what you have, and STRIKE. Nothing here burns on its own; the pile is\n"
        "   inert until the tool in your hand touches it. And the top rung, the timber\n"
        "   a smelt really wants -- you will find you cannot lay it at all.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // What you gathered, and the tool you have not made yet. The dry dead sticks in
    // your satchel are both the fire's fuel AND the stock a spark is carved from.
    TinderState tin = T_NONE;
    bool sticks_laid = false;
    bool have_smolder = false;
    SparkStock stock = STOCK_DRY;      // your hand falls first on a dead dry stick.

    Stage stage = COLD;
    FireTier caught_tier = NO_FIRE;
    double flare_t = 0.0;              // when a tinder flare gives out.
    double t = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "Carve the spark first [c], then lay the kit bottom-up: [t] tinder, [s] sticks.";
    double nag_t = 9;

    bool running = true, quit = false, carried = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'g') {
                if (stage == CAUGHT) { running = false; carried = true; }
                else { nag = "There is no fire in the ring to carry. Strike a caught one first."; nag_t = t + 5; }
            }
            else if (c == 'd') {
                stock = (stock == STOCK_DRY) ? STOCK_GREEN : STOCK_DRY;
                nag = stock == STOCK_DRY
                        ? "Your hand is on a grey dead stick -- snaps dry. This one takes an ember."
                        : "Your hand is on a green shoot -- it bends and weeps. It will only polish.";
                nag_t = t + 5;
            }
            else if (c == 'c') {
                // Carve the spark. make_smolder is the gate: dry stock takes, green does not.
                Wood spindle; spindle.sticks = 1.0;
                spindle.moisture = (stock == STOCK_DRY) ? DRY : DAMP;
                const SmolderKit set = make_smolder(spindle);
                have_smolder = set.made;
                nag = have_smolder
                        ? "You spin a coal into the dead wood and cradle it -- the smolder set is made, and kept."
                        : "The green spindle just polishes and squeals; no smoke, no coal. You need dry dead wood.";
                nag_t = t + 6;
            }
            else if (c == 't') {
                tin = (tin == T_NONE) ? T_DRY : (tin == T_DRY) ? T_DAMP : T_NONE;
                if (stage == FLARING) stage = COLD;   // relaying the nest after a flare.
                nag = tin == T_DRY  ? "You tease a nest of the dry pouch tinder into the ring."
                    : tin == T_DAMP ? "You grab a handful of litter off the damp ground instead -- it feels heavy."
                                    : "You pull the tinder back out of the ring.";
                nag_t = t + 5;
            }
            else if (c == 's') {
                sticks_laid = !sticks_laid;
                nag = sticks_laid ? "You lay the dead sticks over the nest, loose enough to breathe."
                                  : "You clear the sticks back off.";
                nag_t = t + 5;
            }
            else if (c == 'k') {
                // The locked rung. This is the teaching beat, same shape as gather's [f].
                nag = "You reach for a timber log to lay on top -- and there is none. You never "
                      "felled any; the axe is not made.";
                nag_t = t + 6;
            }
            else if (c == 'x') {
                // The strike. firekit.h decides -- the spark must be in hand, and then the
                // fire catches only as high as the ladder reaches. No projection: you learn
                // it here, watching it, exactly as the pit tells its yield only on rake-out.
                const FireKit kit = current_kit(tin, sticks_laid);
                const SmolderKit set{have_smolder};
                const Ignition ig = ignite(kit, set);
                if (!ig.lit) {
                    caught_tier = NO_FIRE;
                    stage = COLD;
                    nag = !have_smolder
                            ? "You go to strike -- and there is no spark in your hand. Where is it? Carve the set first."
                            : "You work the tinder and nothing takes. It is too damp to catch, however you coax it.";
                    nag_t = t + 6;
                } else if (ig.tier == TINDER_FLARE) {
                    caught_tier = ig.tier;
                    stage = FLARING;
                    flare_t = t + FLARE_LIFE;
                    nag = "The nest catches and flares up -- but there is nothing laid over it to take the flame.";
                    nag_t = t + FLARE_LIFE;
                } else {
                    caught_tier = ig.tier;
                    stage = CAUGHT;
                    nag = "The flame climbs off the nest into the sticks and HOLDS. You have a fire.";
                    nag_t = t + 6;
                }
            }
        }

        const double dt = 0.05;
        t += dt;

        // A tinder flare is not a fire; it burns its brief life and dies to nothing,
        // taking the nest with it. You must relay the tinder to try again.
        if (stage == FLARING && t >= flare_t) {
            stage = COLD;
            caught_tier = NO_FIRE;
            tin = T_NONE;   // the nest is spent.
            nag = "The flare gutters out and leaves cooling ash. The nest is spent -- lay fresh tinder.";
            nag_t = t + 6;
        }

        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 37) % N_AMBIENT]; ambient_t = t + 37; }
        if (t > nag_t) nag.clear();

        draw(tin, sticks_laid, have_smolder, stock, stage, caught_tier, t,
             ambient, nag.empty() ? nullptr : nag.c_str());
        nap(dt);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !carried) {
        std::printf("   You leave the kit cold in the stones and walk away.\n\n");
        return 0;
    }

    report_fire(caught_tier);
    std::printf("   It cost you %d minutes and %d seconds at the ring.\n\n",
                (int)t / 60, (int)t % 60);
    return 0;
}
