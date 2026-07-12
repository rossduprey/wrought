// Phase A, the fourth playable slice: fire the ore into metal.
//
// pan.cpp asked whether a wash is a game; dig.cpp, whether a dig is; breaker.cpp,
// whether FREEING the ore is. This file takes what the breaker hands you -- a pan
// concentrate that is still rock, mixing red oxide and dark sulfide copper in one
// heap -- and asks the last question of the front half of the chain: is turning
// ore into METAL a game, or is it a button labelled "smelt"?
//
// It runs on exactly the verbs smelt.h already had -- roast() for the oxidizing
// fire and smelt_copper() for the reducing one -- and adds nothing but a bellows,
// a draft, and a face. No number in here was chosen to make it pleasant; there is
// no score, no target, no win. You get one charge and one fire to think in.
//
// -- The finding this slice is built on. -----------------------------------
//
// smelt.h's copper findings say a reducing furnace has ONE move: it strips oxygen.
// Feed it a sulfide (chalcocite, Cu2S) and it has nothing to take -- the charge
// sits there and goes to slag, copper and all. To win that copper you must first
// put oxygen ON, in a second fire that runs the OTHER way: an oxidizing roast, air
// not charcoal, that burns the sulfur off as SO2 and leaves a copper OXIDE the
// reducing fire can then pour. Two fires of OPPOSITE ATMOSPHERE, in a fixed order:
// roast to desulfurize, then smelt to pour. And it is not the temperature that
// tells them apart -- both run near the same orange heat -- it is the ATMOSPHERE:
// air oxidizes, charcoal reduces. That is the whole decision the draft key makes.
//
// A probe (2026-07-11) on the breaker's own concentrate confirmed it is a game:
//
//   RAW SMELT (reducing only, no roast):   poured 45%   84 g of copper to slag
//   ROAST then SMELT:                       poured 63%   the sulfide cured
//   HEAT: below ~1073 K nothing catches; 1073-1357 K reduces but FREEZES as a
//         slag-tangled sponge; only above copper's melting point (1357 K) does it
//         pour a clean button that leaves the slag behind.
//
// And the probe surfaced a finding the docs had not: roast() only cures the
// sulfide the CRUSHING freed. A chalcocite grain still sealed in gangue -- the
// rock the hammer left whole back at the breaker -- roasts incompletely and is
// left as-is (smelt.h roast(), "honest"). So no fire reaches it, and the recovery
// ceiling here (63%, not 100%) was SET UPSTREAM, at the breaking stone. The
// furnace is where you find out the breaker mattered. That loss is reported, split
// from the sulfide you simply never roasted, because they are different mistakes:
// one is a fire you did not run, the other is a blow you did not strike.
//
// -- The gesture. -----------------------------------------------------------
//
// The pan taught that a persistent display made of the MATERIAL beats one made of
// numbers, and that a decision the player is here to learn must never be stated as
// wisdom. So the furnace's face is the FIRE and the CHARGE, both things a smith
// actually reads: heat shown as COLOUR (dull red, cherry, orange, yellow, white --
// the same scale by which a smith has always judged a fire; grounded, not a gauge),
// the draft shown as what it does to the air, and the charge shown as its materials
// -- red oxide, dark sulfide, stony gangue -- whose proportions you watch SHIFT as
// a roast eats the sulfide away. Nowhere is a projected yield: you set the draft,
// you work the heat, you commit the charge to the fire, and only at the tap do you
// learn what the choice cost. Showing "recovery would be higher if you roasted"
// would hand over the very thing the fire is here to teach.
//
// Build: make furnace   (needs a terminal; it reads single keypresses)

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
#include "smelt.h"
#include "char.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// The fire's pace. These are the furnace's counterpart to the breaker's TAP_BLOW
// and the pan's STROKE: authored rates that set how the work FEELS and that no
// test or finding reads. The onsets they play against (roast/reduce at 1073 K,
// clean pour at copper's 1357 K melting point) are NOT authored -- they are
// smelt.h's tabulated envelope and IUPAC melting point. Only the bellows and the
// cooling are the game's pacing.
static constexpr double HEARTH_START = 700.0;   // K, a banked bed of embers: dull red
static constexpr double BELLOWS_PUMP = 48.0;    // K a stroke adds. AUTHORED pacing.
static constexpr double COOL_PER_SEC = 11.0;    // K/s the fire sheds when you are not blowing. AUTHORED.
static constexpr double HEAT_MAX     = 1650.0;  // K, as hot as this hearth will go
static constexpr double HEAT_FLOOR   = 500.0;   // K, embers never quite die while you tend them

// A grain has to weigh something before the panel admits it exists.
static constexpr double SPECK = 1e-6;

// The reductant for the reducing fire -- now MADE, not conjured. This closes the
// loop the tool-bootstrap named: the charcoal every furnace burns is a pit of wood
// charred down (char.h), and that wood was gathered by hand (fuel.h's Wood, the
// stock gather.cpp wins). The old charcoal_bed() spawned a flat 2 kg of carbon and
// said so out loud -- "kept ample on purpose" -- and that scaffold hid the very
// thing this file was built to teach at the end. Here the bed comes off a SEALED
// pit of hand-pulled dead sticks: no axe, no felled timber in it. It is still not
// the binding constraint, but now for an HONEST reason rather than a fiat one -- a
// copper reduction sips only a fraction of the carbon even so small a pit yields
// (CARBON_PER_CU is grams per kg of copper, not kilos). The reductant gate is
// cheap; the fire's real fuel cost is HEAT -- the bellows here, and the bloomery's
// blast for iron -- not the carbon in the charge. The tap says so.
//
// And the pit is a FIRE, not a heap: it must be LIT before it chars, with a spark
// that is itself a made tool (firekit.h). So the bank carries tinder to catch, and
// the pit is struck with a smolder kit the smith carved -- no free match. By the
// copper furnace the kit is long since possessed, like the pick, so this always
// lights; but it lights honestly now, through ignite(), not by fiat.
static constexpr double PIT_TINDER   = 0.2;  // kg dry litter to catch the pit. AUTHORED pacing.
static constexpr double PIT_STICKS   = 3.0;  // kg hand-pulled sticks banked in the pit. AUTHORED pacing.
static constexpr double PIT_MOISTURE = 0.15; // as gather.cpp wins it, green off the tree. AUTHORED pacing.
static Substance pit_charcoal(const SmolderKit& spark) {
    FireKit w; w.tinder = PIT_TINDER; w.sticks = PIT_STICKS; w.moisture = PIT_MOISTURE;
    return char_pit(w, spark, PIT_SEALED);
}

static bool is_copper_ore(int p) { return p == CUPRITE || p == CHALCOCITE; }

// Copper still held as sulfide sealed inside gangue -- the composite chalcocite the
// hammer never freed. A roast cannot reach it (smelt.h roast() converts only free
// chalcocite), so this is the copper the furnace CANNOT win no matter the fire: the
// breaker's unpaid debt, come due. COMPOSITE_TARGET_FRACTION of a composite grain
// is the mineral; that fraction of the copper in it is what is lost this way.
static double locked_sulfide_copper(const Substance& s) {
    double cu = 0.0;
    for (int b = 0; b < N_SIZE; ++b)
        cu += COMPOSITE_TARGET_FRACTION * s.composite[CHALCOCITE][b]
              * element_fraction(CHALCOCITE, EL_CU);
    return cu;
}

// ---------------------------------------------------------------------------
// Terminal. Lifted from breaker.cpp (which lifted it from dig.cpp, from pan.cpp) --
// a raw tty with a non-blocking read is the one piece of machinery every slice
// needs and none should reinvent.

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
// What the eye reads off a fire: its COLOUR. A smith has always judged furnace
// heat by the colour of the glow, and the scale is real -- dull red near 750 K,
// cherry near 950, orange past 1100, yellow past 1350, white above. It is not a
// coincidence that the roast/reduce onset (1073 K) lands at the first true orange
// and the clean pour (copper's 1357 K melt) at yellow-white: those are the same
// physics the colours report. So the colour IS the gauge, honestly. No number of
// kelvin appears on the panel -- only the light.
static const char* heat_colour(double T) {
    if (T < 750.0)  return "dark, barely warm";
    if (T < 900.0)  return "a dull red";
    if (T < 1073.0) return "a deep cherry red";
    if (T < 1200.0) return "orange";
    if (T < 1357.0) return "a hot bright orange";
    if (T < 1500.0) return "yellow, near white";
    return "a white glare that hurts to watch";
}

static const int PANEL_COL = 46;

static void draw_panel(const Substance& charge, double heat, bool reducing) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("the fire and the charge");
    line("%s", "------------------------------");

    // The heat, as colour and a glow bar. The bar fills between the ember floor
    // and the hottest this hearth reaches -- a thing you can see brightening as
    // you blow, not a temperature you read off a dial.
    line("the fire burns %s", heat_colour(heat));
    char glow[21];
    const double span = (heat - HEAT_FLOOR) / (HEAT_MAX - HEAT_FLOOR);
    const int fill = (int)std::lround((span < 0 ? 0 : span > 1 ? 1 : span) * 20.0);
    for (int i = 0; i < 20; ++i) glow[i] = i < fill ? '#' : '.';
    glow[20] = '\0';
    line("  [%s]", glow);
    line("%s", "");

    // The draft: the whole decision, in one line. Air eats (oxidizes, a roast);
    // charcoal starves (reduces, a smelt). Which one is running is which fire the
    // next commit will be.
    if (reducing)
        line("the draft is BANKED under charcoal:");
    else
        line("the draft is OPEN to the air:");
    line("  %s", reducing ? "a starved, reducing fire -- it strips oxygen (a smelt)"
                          : "a hungry, oxidizing fire -- it adds oxygen (a roast)");
    line("%s", "");

    // The charge, as its materials. You watch these proportions SHIFT: a roast
    // eats the dark sulfide and the red oxide grows in its place, and that is the
    // roast working, shown as the stuff itself, not as a readout of what it did.
    line("in the crucible:");
    const double ox = charge.phase_mass(CUPRITE);
    const double su = charge.phase_mass(CHALCOCITE);
    const double gangue = charge.total_mass() - ox - su;
    auto matbar = [&](const char* name, double m) {
        const double tot = charge.total_mass();
        const double frac = tot > SPECK ? m / tot : 0.0;
        char b[17];
        const int pf = (int)std::lround(frac * 16.0);
        for (int i = 0; i < 16; ++i) b[i] = i < pf ? '=' : ' ';
        b[16] = '\0';
        line("  %-13s |%s| %4.0f g", name, b, m * 1000.0);
    };
    matbar("red oxide", ox);
    matbar("dark sulfide", su);
    matbar("stony gangue", gangue > 0 ? gangue : 0.0);
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the bellows wheeze and the coals breathe brighter with each stroke",
    "heat comes off the hearth in a wall; your face is tight with it",
    "green flame licks up where something in the ore is burning",
    "the charcoal ticks and settles as it burns down",
    "your eyes water from the smoke and you blink the fire back into focus",
    "the crucible glows from within now, not just the coals around it",
    "sweat runs and dries before it falls; the front of you is baking",
    "the draught roars when you open it and sighs when you bank it",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(const Substance& charge, double heat, bool reducing, int roasts,
                 double t, const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-48s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);

    std::printf("   the furnace, and the concentrate you panned at the breaking stone\n\n");
    std::printf("   fires roasted so far: %d\n\n", roasts);

    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [b] work the bellows   [d] open/bank the draft\n");
    std::printf("   [f] commit the charge to the fire   [q] rake it out and stop\n");

    draw_panel(charge, heat, reducing);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The tap. Only what a scale and an eye at the mouth of the furnace can tell you:
// how much metal ran out, whether it poured a clean button or froze a spongy mass
// tangled with slag, how much of the charge's copper you won, and -- the two ways
// you can leave copper behind -- the sulfide you never roasted, and the sulfide the
// hammer never freed. The finding is stated only after, as consequence.
static void report_pour(const MeltResult& r, double cu_feed, double locked_su,
                        int roasts) {
    const double cu_won = r.metal_cu;
    const double recov  = cu_feed > SPECK ? cu_won / cu_feed : 0.0;
    const double to_slag = cu_feed - cu_won;

    std::printf("   You break the fire open and look at what ran down.\n\n");

    if (cu_won * 1000.0 < 0.5) {
        std::printf("   Nothing pours. ");
        if (!r.lit)
            std::printf("The fire never caught hot enough to reduce the\n"
                        "   ore -- it sat there as rock. You needed it orange, at least.\n\n");
        else
            std::printf("There was no oxide in the charge for the fire to\n"
                        "   reduce -- only sulfide, which a reducing fire cannot touch.\n"
                        "   It has gone to slag. You never roasted it.\n\n");
        return;
    }

    if (r.molten)
        std::printf("   A clean button of copper freezes in the bottom -- %.0f g of\n"
                    "   metal, poured liquid and left the slag above it.\n\n",
                    cu_won * 1000.0);
    else
        std::printf("   You win %.0f g of copper, but the fire never cleared its\n"
                    "   melting point: it came out a spongy mass shot through with slag,\n"
                    "   not a clean pour. Hotter, and it would have run.\n\n",
                    cu_won * 1000.0);

    std::printf("   That is %.0f%% of the copper the concentrate held.\n\n", 100.0 * recov);

    if (to_slag * 1000.0 > 0.5) {
        // Split the loss into the two distinct mistakes.
        const double never_roasted = (roasts == 0) ? (to_slag - locked_su) : 0.0;
        if (never_roasted * 1000.0 > 0.5)
            std::printf("   %.0f g of copper went to slag as sulfide the reducing fire\n"
                        "   could not touch. A reducing fire only strips oxygen, and the\n"
                        "   sulfide had none to give. Roasted first -- burned in open air\n"
                        "   to drive the sulfur off -- it would have smelted like the oxide.\n\n",
                        never_roasted * 1000.0);
        if (locked_su * 1000.0 > 0.5)
            std::printf("   And %.0f g stayed locked as sulfide sealed inside gangue -- rock\n"
                        "   the hammer never split. No fire reaches a grain the breaker left\n"
                        "   whole; that copper was lost back at the breaking stone, not here.\n\n",
                        locked_su * 1000.0);
    }

    if (roasts > 0)
        std::printf("   The roast was the whole of it: you added oxygen so the fire could\n"
                    "   take it off. Two fires, opposite in what they do, in the one order\n"
                    "   that works -- and nobody was going to tell you which came first.\n\n");
    else
        std::printf("   You ran one fire where the ore wanted two. The oxide gave up its\n"
                    "   copper; the sulfide never would, not to a fire that only reduces.\n\n");

    // The loop-close. The charcoal that reduced the ore was not laid beside it by
    // fiat; it was made, and cheaply -- which is the last thing this fire teaches.
    std::printf("   And the charcoal you banked to reduce it was not free. It was a pit\n"
                "   of dead sticks, pulled from a standing tree by hand and charred down --\n"
                "   no axe, no felled timber in it -- and even that pit had to be LIT: no\n"
                "   spark catches a log, and the spark itself was a friction set you carved,\n"
                "   not a match you were handed. It gave carbon to spare all the same. A\n"
                "   copper reduction sips only a fraction of what so small a pit yields; the\n"
                "   fire's real cost was the HEAT you pumped, not the carbon in the charge.\n"
                "   The reductant gate is cheap. The fuel wall bites elsewhere -- at the\n"
                "   bloom, where iron's blast eats charcoal by the basketful.\n\n");
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "furnace: needs a terminal. Run it yourself: cd core && make furnace\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You are at the furnace with the concentrate you panned at the breaking\n"
        "   stone. It is still rock -- but rich rock, and two kinds of copper in it:\n"
        "   a red oxide and a dark sulfide, mixed in the one heap.\n\n"
        "   The fire does one thing, and which thing depends on the DRAFT. Bank it\n"
        "   under charcoal and starve it and it REDUCES -- it strips oxygen off the\n"
        "   ore and pours metal. Open it to the air and it OXIDIZES -- it burns, and\n"
        "   adds oxygen. The red oxide only wants the reducing fire. But the dark\n"
        "   sulfide has no oxygen to strip: a reducing fire cannot touch it. To win\n"
        "   its copper you must first ROAST it in open air to burn the sulfur off --\n"
        "   and only then can the reducing fire pour it.\n\n"
        "   Work the bellows to bring the heat up; the colour of the fire is how you\n"
        "   read it. When you like the fire, commit the charge to it. How many fires\n"
        "   to run, and in what order, is the whole of it -- and nobody will tell you.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // The charge: the breaker's concentrate. We materialize it the way the breaker
    // materializes its dig -- dig the mixed copper column, crush it near the sweet
    // spot, pan it -- so this slice stands alone but is fed exactly what the one
    // before it produces: a heap that mixes freed oxide, freed sulfide, and sulfide
    // still locked in gangue.
    Substance pile = dig_column(Place{0.0, 0.0}, 0.4);
    for (int i = 0; i < 3; ++i) pile = crush(pile, 0.40);
    Substance charge = separate(pile, PAN).concentrate;

    // What the furnace is being asked to win, fixed at the start so the tap can be
    // honest about the ceiling.
    double cu_feed = 0.0;
    for (int p = 0; p < N_PHASE; ++p) if (is_copper_ore(p)) cu_feed += charge.phase_mass(p);
    const double locked_su = locked_sulfide_copper(charge);

    // The spark, carved once and carried. By the copper furnace the smith has long
    // climbed the staircase, so the kit is in hand -- made, per firekit.h, from dry
    // dead stock, and it does not wear. It is what lights the char pit below.
    Wood dry_stock; dry_stock.sticks = 0.1; dry_stock.moisture = 0.15;
    const SmolderKit spark = make_smolder(dry_stock);

    double heat = HEARTH_START;
    bool reducing = false;   // a bare hearth is open to the air
    int roasts = 0;
    double t = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "You bank the coals under the crucible and take up the bellows.";
    double nag_t = 6;

    bool running = true, quit = false, poured = false;
    MeltResult pour;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'b') {
                heat += BELLOWS_PUMP;
                if (heat > HEAT_MAX) heat = HEAT_MAX;
                t += 1.2;
            }
            else if (c == 'd') {
                reducing = !reducing;
                nag = reducing
                    ? "You bank charcoal over the charge and choke the air. The fire "
                      "goes hungry -- it will strip oxygen now, and reduce."
                    : "You rake the charcoal back and open the draught. The fire "
                      "breathes air -- it will burn, and add oxygen: a roast.";
                nag_t = t + 4;
            }
            else if (c == 'f') {
                if (reducing) {
                    // The smelt: terminal. It pours what it can and the charge is spent.
                    pour = smelt_copper(charge, pit_charcoal(spark), heat);
                    if (!pour.lit) {
                        nag = "The fire is too cool to catch -- the ore just sits in it. "
                              "Bring it up to orange at least.";
                        nag_t = t + 4;
                        t += 4.0;
                    } else {
                        running = false; poured = true;
                    }
                } else {
                    // The roast: oxidizing, non-terminal. It cures the free sulfide.
                    if (heat < ROAST_T) {
                        nag = "Too cool to roast -- an open fire this cool only crusts the "
                              "ore with sulfate and the sulfur stays. Hotter.";
                        nag_t = t + 4;
                        t += 4.0;
                    } else {
                        RoastResult ro = roast(charge, heat);
                        const bool cured = ro.gas[EL_S] > SPECK;
                        charge = ro.calcine;
                        ++roasts;
                        t += 20.0;
                        nag = cured
                            ? "An acrid yellow reek rolls off the fire -- the sulfur, "
                              "burning off as it should. The dark ore is going red."
                            : "The fire roars in the open air, but nothing reeks off it. "
                              "There is no free sulfide left in the charge to burn.";
                        nag_t = t + 5;
                    }
                }
            }
        }

        t += 0.05;
        heat -= COOL_PER_SEC * 0.05;
        if (heat < HEAT_FLOOR) heat = HEAT_FLOOR;
        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 41) % N_AMBIENT]; ambient_t = t + 41; }
        if (t > nag_t) nag.clear();

        draw(charge, heat, reducing, roasts, t, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(0.05);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !poured) {
        std::printf("   You rake the fire out and leave the charge to cool on the hearth.\n\n");
        return 0;
    }

    report_pour(pour, cu_feed, locked_su, roasts);
    std::printf("   It cost you %d minutes and %d seconds at the bellows.\n\n",
                (int)t / 60, (int)t % 60);
    return 0;
}
