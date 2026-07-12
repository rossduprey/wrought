// Phase A, the sixth playable slice: burn a pit of wood down to charcoal.
//
// pan/dig/breaker/furnace/carry each took one verb of the chain and asked whether
// it was a game. This one takes the verb that feeds the furnace its reductant: you
// have a heap of wood, and the furnace does not eat wood -- it eats CHARCOAL, and
// charcoal is made, in a pit, by you. So this asks: is making the fuel a game, or
// is it a button labelled "char"?
//
// It runs on char.h's station -- pyrolyze() for the yield, PitSeal for the one gate
// -- and adds nothing but a fire that lives in real time, a cover you can work, and
// a face. No score, no target, no projected yield. You bank the wood, you light it,
// and the whole of it is the one decision char.h names.
//
// -- The finding this slice is built on. -----------------------------------
//
// char.h: THE PIT'S TRICK IS STARVING THE FIRE THAT MAKES THE CHAR. Pyrolysis needs
// heat, and the only heat here is burning wood -- so the same fire that chars the
// charge will, given air, burn the CHARCOAL too, back to ash. A char pit is a fire
// you SUFFOCATE. The char you carry is not what the wood held; it is what you KEPT
// from the fire, and the seal decides how much.
//
// The gesture makes that a matter of TIMING, which is the charcoal-burner's whole
// craft and the thing char.h's static seal_retention() abstracts away: to carbonise
// at all the pit must first get HOT, and heat needs air -- so you cannot just seal a
// cold pile and walk off, it will only sit there. You open it to catch and build the
// fire; then, once it is hot and smoking, you SMOTHER it, and that is the move that
// feels wrong -- you are putting out a good fire -- and is exactly right. Leave it
// breathing and it eats itself to ash; seal it too soon and it goes cold with the
// wood still wood. The warm-up-then-smother is game pacing (the constants below,
// which no test reads); the SIGN it serves -- more air, less char -- is char.h's.
//
// -- The gesture. -----------------------------------------------------------
//
// The furnace taught that the fire's own COLOUR is an honest gauge and a projected
// yield is a cheat. So the pit's face is its SMOKE and FLAME: open flame means it is
// burning DOWN (you watch the pile shrink), thick blue smoke with no flame means it
// is CARBONISING, thinning pale smoke means it is going cold. You read the fire, you
// work the cover, and only when you rake it out do you learn what your timing cost.
// Nowhere does it say "seal it now": that is the sentence the pit exists to teach.
//
// Build: make char   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "char.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// The pit's pace. Authored game pacing -- the counterpart to furnace.cpp's bellows
// and cooling -- setting how the burn FEELS. No test or finding reads these. What
// they play against IS a finding: CHAR_YIELD (fuel.h) is how much of the wood can
// ever come out as carbon, and these rates only decide how fast you get there and
// how fast the fire eats it back if you let it breathe.
static constexpr double HEAT_START  = 640.0;   // K, a pile just catching -- some flame, not yet hot through.
static constexpr double CARB_ONSET  = 720.0;   // K, below this the pit only smoulders and chars nothing.
static constexpr double HEAT_FLOOR  = 400.0;   // K, a smothered pit sheds to here and sits.
static constexpr double AIR_CEIL    = 1050.0;  // K, an open pit roars up to about here.
static constexpr double CHOKE_CEIL  = 760.0;   // K, a leaky cover holds only a low fire.
static constexpr double AIR_RISE    = 26.0;    // K/s the fire climbs toward its ceiling with air.
static constexpr double SEAL_FALL   = 14.0;    // K/s a sealed pit loses -- no air, the fire sinks.

// Per-second mass rates, in kg. CARBONISE turns wood into char (at CHAR_YIELD);
// BURN is the char and wood an OPEN fire oxidises to ash. That BURN is the whole
// finding made kinetic: air present, the product goes back to gas.
static constexpr double CARBONISE   = 0.070;   // kg of wood/s the hot pit chars.
static constexpr double BURN_OPEN   = 0.110;   // kg/s an open, hot pit eats -- wood and made char alike.
static constexpr double BURN_VENT   = 0.028;   // kg/s a leaky cover still loses.

static constexpr double SPECK = 1e-6;

// ---------------------------------------------------------------------------
// Terminal. The same raw tty every slice shares (pan -> dig -> breaker -> furnace);
// a non-blocking single-key read and nothing more.

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
// What the eye reads off a pit: its smoke and flame. Like the furnace's colour,
// this is the honest gauge -- the thing a charcoal-burner actually watches -- and no
// number of kelvin appears on the panel. Open flame is fuel leaving as fire; thick
// blue smoke is the volatiles driving off and the carbon staying; pale thin smoke is
// a fire going out. Which one you see is which way the mass is moving.
static const char* fire_face(PitSeal seal, double heat) {
    const bool hot = heat >= CARB_ONSET;
    switch (seal) {
        case PIT_OPEN:
            return hot ? "open flame leaps off the pile -- it is burning down"
                       : "a young fire licks up through the wood, catching";
        case PIT_VENTED:
            return hot ? "low flame gutters under the cover, smoke rolling off the seams"
                       : "smoke seeps from the seams; the fire is low and uncertain";
        case PIT_SEALED:
            return hot ? "no flame now -- thick blue smoke pours off; it is carbonising"
                       : "the smoke thins and pales; the sealed pit is going cold";
    }
    return "";
}

static const int PANEL_COL = 46;

static void draw_panel(double wood_left, double char_made, double wood_banked,
                       PitSeal seal, double heat) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("the pit");
    line("%s", "------------------------------");
    line("%s", fire_face(seal, heat));

    // The heat, as a glow bar between a smothered pit and a roaring one. You watch
    // it climb when you give it air and sink when you choke it -- never a number.
    char glow[21];
    const double span = (heat - HEAT_FLOOR) / (AIR_CEIL - HEAT_FLOOR);
    const int fill = (int)std::lround((span < 0 ? 0 : span > 1 ? 1 : span) * 20.0);
    for (int i = 0; i < 20; ++i) glow[i] = i < fill ? '#' : '.';
    glow[20] = '\0';
    line("  [%s]", glow);
    line("%s", "");

    // The two masses, as the stuff itself. The pile shrinks as it chars (or burns);
    // the charcoal grows as it is made (or drops back to ash if you let it breathe).
    // Both scaled to the wood you banked, so the bars are honest against each other.
    auto matbar = [&](const char* name, double m) {
        const double frac = wood_banked > SPECK ? m / wood_banked : 0.0;
        char b[17];
        const int pf = (int)std::lround((frac < 0 ? 0 : frac > 1 ? 1 : frac) * 16.0);
        for (int i = 0; i < 16; ++i) b[i] = i < pf ? '=' : ' ';
        b[16] = '\0';
        line("  %-13s |%s| %5.0f g", name, b, m * 1000.0);
    };
    matbar("wood, still", wood_left > 0 ? wood_left : 0.0);
    matbar("charcoal", char_made > 0 ? char_made : 0.0);
    line("%s", "");

    // The one decision, in a line: what the cover is doing to the air right now.
    const char* what =
        seal == PIT_OPEN   ? "OPEN -- air feeds the fire; it burns, and eats its own char"
      : seal == PIT_VENTED ? "VENTED -- a leaky cover; it chars, but still loses some"
                           : "SEALED -- choked to a smoulder; it chars and keeps it";
    line("the cover is %s", what);
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "smoke stands off the mound in a slow blue column and bends with the air",
    "the pile ticks and shifts as the wood settles into itself",
    "you walk the pit banking a loose seam with a bootful of earth",
    "the smell of it changes -- sweet woodsmoke going sharp and tarry",
    "heat comes off the covered mound in a haze you can see through",
    "a seam glows orange for a moment before you tamp it shut",
    "the smoke pulls thin and white where the cover has cracked",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(double wood_left, double char_made, double wood_banked,
                 PitSeal seal, double heat, double t, const char* ambient,
                 const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);
    std::printf("   the charcoal pit -- a mound of wood you mean to carry off as fuel\n\n");
    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [c] work the cover: loosen it / choke it down\n");
    std::printf("   [r] rake out the charcoal and stop   [q] walk away\n");
    draw_panel(wood_left, char_made, wood_banked, seal, heat);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The rake-out. Only what a scale and an eye at an opened pit can tell you: how much
// charcoal came out, against how much wood went in. The finding is stated after, as
// consequence -- and split by WHICH way you lost it, because leaving it open and
// drawing it early are different mistakes.
static void report_draw(double char_made, double wood_banked, double wood_left,
                        double ideal_char, PitSeal seal_at_draw) {
    const double ratio = wood_banked > SPECK ? char_made / wood_banked : 0.0;

    std::printf("   You break the mound open and rake out what is in it.\n\n");

    if (char_made * 1000.0 < 1.0) {
        std::printf("   Ash and a few charred brands, no charcoal to speak of. ");
        if (seal_at_draw == PIT_OPEN)
            std::printf("You never\n   covered it -- it burned like any bonfire and left you what a\n"
                        "   bonfire leaves.\n\n");
        else
            std::printf("It never got\n   hot enough to char through, or you smothered it before it did.\n\n");
        return;
    }

    std::printf("   %.0f g of charcoal, out of the %.0f g of wood you banked -- about\n"
                "   %.0f%% of the pile came back as fuel.\n\n",
                char_made * 1000.0, wood_banked * 1000.0, 100.0 * ratio);

    // Near the ideal yield (a fifth of the DRY wood): the pit was run right. Judged
    // against what pyrolysis could give this pile, not against the raw wood -- the
    // water was always going to leave.
    if (char_made > 0.9 * ideal_char) {
        std::printf("   That is about all a pit gives -- roughly a fifth. You kept it choked\n"
                    "   while it charred, so the fire made the char and did not turn round and\n"
                    "   eat it. The rest left as smoke; it was always going to.\n\n");
    } else if (wood_left * 1000.0 > 5.0) {
        // Drawn early: wood still on the pile.
        std::printf("   There is still wood on the pile -- brands, half-charred, not yet\n"
                    "   carbon. You opened it too soon. Given more time choked, they would\n"
                    "   have come through.\n\n");
    } else {
        // The pile is gone but little char: it was let breathe and burned to ash.
        std::printf("   The pile is gone but little of it is charcoal -- you let it breathe,\n"
                    "   and an open fire burns the char as readily as the wood. What should\n"
                    "   have been fuel went up as ash. The seal, not the wood, was the yield.\n\n");
    }

    std::printf("   What you drew is free carbon -- the exact reductant the furnace strips\n"
                "   oxygen with, and the fuel it never told you it was spending. A fifth of\n"
                "   a woodpile per firing: that is the cost behind every bloom.\n\n");
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "char: needs a terminal. Run it yourself: cd core && make char\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You have a heap of wood, and a furnace downhill that will not eat it.\n"
        "   The furnace eats CHARCOAL -- wood with the smoke and water cooked out of\n"
        "   it, until only the carbon is left. You make that here, in a pit, and the\n"
        "   making has one trick.\n\n"
        "   The heat that chars the wood comes from burning some of it, so the pit\n"
        "   must have air to catch and come up hot. But the same fire, left breathing,\n"
        "   goes on to burn the charcoal too -- back to ash. So a char pit is a fire\n"
        "   you light, bring up hot, and then SMOTHER: choke it down to a smoulder that\n"
        "   chars the wood without consuming it, and hold it there until it is through.\n\n"
        "   Open it too long and you sweep up ashes. Seal it while it is still cold and\n"
        "   it only sits there. When to choke it, and when to break it open, is the\n"
        "   whole of it -- and nobody will tell you.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // The pile you bank: dead sticks and split timber, dry enough to catch. Wood.h's
    // stock, fed straight to the pit. Moisture comes off the yield, not modelled as a
    // separate drying stage here -- this pile is seasoned.
    Wood pile;
    pile.sticks   = 3.0;
    pile.timber   = 3.0;
    pile.moisture = 0.10;
    const double wood_banked = pile.burnable();

    double wood_left = wood_banked;   // not yet charred or burned
    double char_made = 0.0;           // carbon kept, in kg
    double heat = HEAT_START;
    PitSeal seal = PIT_OPEN;          // you light it in the open to catch
    double t = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "You touch a brand to the pile and it catches. Let it get hot before you cover it.";
    double nag_t = 7;

    bool running = true, quit = false, drew = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'r') { running = false; drew = true; }
            else if (c == 'c') {
                seal = (seal == PIT_OPEN)   ? PIT_VENTED
                     : (seal == PIT_VENTED) ? PIT_SEALED
                                            : PIT_OPEN;
                nag = seal == PIT_SEALED
                        ? "You heap earth over the mound and choke the last vents. The flame dies to smoke."
                    : seal == PIT_VENTED
                        ? "You draw a loose cover over it, leaving the seams open. The fire drops but holds."
                        : "You pull the cover back and the pile takes air -- flame jumps up through it.";
                nag_t = t + 5;
            }
        }

        const double dt = 0.05;
        t += dt;

        // The fire seeks a ceiling set by the air it gets: open roars up, sealed
        // sinks to a smoulder, vented holds a low flame.
        if (seal == PIT_OPEN) {
            if (heat < AIR_CEIL) heat += AIR_RISE * dt;
        } else if (seal == PIT_VENTED) {
            if (heat < CHOKE_CEIL) heat += AIR_RISE * dt * 0.5;
            else                   heat -= SEAL_FALL * dt * 0.5;
        } else { // SEALED: no air, the fire sinks
            heat -= SEAL_FALL * dt;
        }
        // A fire with nothing left to burn cannot stay hot.
        if (wood_left <= SPECK && char_made <= SPECK) heat -= SEAL_FALL * dt;
        if (heat < HEAT_FLOOR) heat = HEAT_FLOOR;

        // The mass moves only when the pit is hot enough to work.
        if (heat >= CARB_ONSET) {
            // Carbonise: wood becomes char at the pyrolysis yield. Any air-fed pit
            // chars; it is what the pit is FOR.
            if (wood_left > 0.0) {
                double dw = CARBONISE * dt;
                if (dw > wood_left) dw = wood_left;
                wood_left -= dw;
                // char.h/fuel.h own the yield: one kg of wood leaves CHAR_YIELD as carbon.
                char_made += pyrolyze(dw, pile.moisture).phase_mass(CARBON);
            }
            // Burn: an open (or leaky) fire oxidises what is there -- the made char
            // first, since it is exposed on the burning face -- back to ash. This is
            // seal_retention()'s SIGN as a rate: more air, less char kept.
            const double burn = seal == PIT_OPEN ? BURN_OPEN
                              : seal == PIT_VENTED ? BURN_VENT : 0.0;
            if (burn > 0.0) {
                double db = burn * dt;
                // Eat the char it already made, then bite into the raw wood.
                const double from_char = db < char_made ? db : char_made;
                char_made -= from_char;
                db -= from_char;
                if (db > 0.0) { wood_left -= db; if (wood_left < 0) wood_left = 0; }
            }
        }

        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 37) % N_AMBIENT]; ambient_t = t + 37; }
        if (t > nag_t) nag.clear();

        draw(wood_left, char_made, wood_banked, seal, heat, t,
             ambient, nag.empty() ? nullptr : nag.c_str());
        nap(dt);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !drew) {
        std::printf("   You leave the pit smoking on the hillside and walk away.\n\n");
        return 0;
    }

    report_draw(char_made, wood_banked, wood_left,
                pyrolyze(wood_banked, pile.moisture).phase_mass(CARBON), seal);
    std::printf("   It cost you %d minutes and %d seconds at the pit.\n\n",
                (int)t / 60, (int)t % 60);
    return 0;
}
