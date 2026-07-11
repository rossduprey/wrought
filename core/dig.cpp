// Phase A, the second playable slice: work the ground.
//
// pan.cpp answered "is the grade/recovery tradeoff a game?" for Era 0. This file
// asks the sibling question for the very front of the chain, the one geology.h
// built and nothing yet lets you *do*: is DIGGING a game, or is it a button that
// hands you a bag of ore? The pan taught that a wash is a continuous gesture with
// one decision (when to stop). The dig has to find its own gesture, because a hole
// is not a swirl.
//
// It runs on exactly the same field as the geology tests -- sample(), dig_column()
// -- and adds nothing but legibility. No number in here was chosen to make the
// experience pleasant, and there is no score, no target, and no win. (DESIGN.md:
// a physical detail earns its place if it is true, creates a decision, and is
// legible without a wiki.)
//
// -- The gesture, and why this one. -----------------------------------------
//
// Decided with Ross, and until now carried only in conversation (RUNBOOK.md warns
// that is not a place a decision survives): the player sees a **bar that shows
// which layer he is working**, and as he strikes the ground with the hand or the
// shovel he watches, in real time, the pit cross those layers. That is the whole
// input model. It is the honest terminal translation of a left-click that eats
// into the dirt -- one strike is one impulse (the pan's [space] again, because a
// terminal cannot see a held mouse button any more than it can see a held swirl),
// and the bar is the persistent display the pan taught us a process needs.
//
// The bar is vertical because depth is the axis that matters here, and depth is
// the one thing geology.h says a dig is *for*: it does not just get you more, it
// gets you a DIFFERENT MINERAL. A copper body is a weathered oxide cap over a
// barren leached middle over a primary sulfide root (geology.h). So the panel does
// not merely climb as you dig -- it changes character, gaining a sulfide the
// furnace will refuse until it is roasted. Dig shallow for a small clean oxide
// pile; dig deep for a big mixed one you will have to sort by eye and smelt in two
// fires. That choice is the decision the slice exists to make legible, and it is
// never stated, only dug.
//
// -- What the ground gives, and what it withholds. --------------------------
//
// Every scoop comes up as LOCKED ROCK (geology.h): the dig fills the composite
// bins, so the assay reads a *grade* -- how much copper is in this ground -- but
// yields nothing a separator can win. The rock stays rock in the hand and in the
// cart. Only the rock-breaking station (separate.h crush) frees the grains, and
// that station is a place you walk to. This slice therefore ends where the honest
// chain says it must: you shoulder the pile and leave to go break it. The panel is
// honest about a grade it cannot yet let you keep.
//
// And the valley is mostly barren country rock. Prospecting is walking it and
// watching for the spot where the numbers twitch. Copper ground and tin ground sit
// hundreds of meters apart, so no single hole yields both -- co-location, made
// footsteps. You can walk to the tin, but you will feel how far it is.
//
// Build: make dig   (needs a terminal; it reads single keypresses)

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

using namespace wrought;

// ---------------------------------------------------------------------------
// The tools, and the ground's resistance to them.
//
// These are the dig's counterpart to the pan's STROKE and DRAG_TAU: authored
// rates that set how fast the work goes and NOTHING a test or a finding reads.
// Depth is measured in tiers (1.0 == one DepthTier crossed), so a strike advances
// the pit by a fraction of a tier and bites some mass of that tier's rock into the
// spoil pile. The shovel is the classic Era-0-into-1 rung: it multiplies what you
// pull out of the ground (~10x) AND drives the pit down faster -- which is exactly
// why a shovel with no cart just buries you in rock you cannot carry (DESIGN.md).
// The bite mass is passed straight to geology.sample() as its `mass` argument, so
// the shovel changes how MUCH rock of a given makeup you take, never the makeup.
static constexpr double HAND_STEP   = 0.12;  // tiers of depth per bare-hand strike. AUTHORED.
static constexpr double HAND_BITE   = 0.05;  // kg of rock per bare-hand strike. AUTHORED.
static constexpr double SHOVEL_STEP = 0.50;  // tiers per shovel strike (~4x deeper). AUTHORED.
static constexpr double SHOVEL_BITE = 0.50;  // kg per shovel strike (10x the hand). AUTHORED.

static constexpr double WALK_STEP   = 10.0;  // m per step across the valley floor. AUTHORED.
static constexpr double MAX_DEPTH   = (double)N_TIER; // you strike bedrock at the bottom tier.

// A grain has to weigh something before the eye and the panel admit it exists.
static constexpr double SPECK = 1e-6;

// The tier a pit of this depth is cutting. Clamped: at bedrock you keep working
// the deepest tier, you do not fall through the world.
static int tier_at(double depth) {
    int t = (int)std::floor(depth);
    if (t < 0) t = 0;
    if (t >= N_TIER) t = N_TIER - 1;
    return t;
}

// ---------------------------------------------------------------------------
// What the character can see of the ground he is standing on, before he has
// broken anything. He has eyes; the seams in a cut bank look different by depth,
// and that is all the bar tells him. It does not name the mineral -- that is the
// assay's job (asking what is in the dirt is the panel, README claim #3), and the
// bar is only the face of the pit.
static const char* tier_face(int t) {
    switch (t) {
        case SURFACE: return "weathered cap  (soft, rusty-stained)";
        case MIDDLE:  return "leached ground (pale, picked-over)";
        default:      return "hard rock      (fresh, unweathered)";
    }
}

// ---------------------------------------------------------------------------
// Terminal. Lifted wholesale from pan.cpp -- a raw tty with a non-blocking read
// is the one piece of machinery both slices need and neither should reinvent.

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
// The spoil pile, read out. One row per mineral, heaviest first, fixed at first
// appearance so the rows do not swap under your eyes (the pan's Assay, again).
//
// There is no recovery column here, and that is deliberate, not an omission. A
// wash REJECTS mass, so it has two numbers that trade against each other. A dig
// only ADDS -- nothing is being thrown away yet -- so the honest state is the
// composition vector itself (RUNBOOK: the vector, not a score, is the state) plus
// the one thing the ground is hiding: every gram of it is locked, and the grade
// you can read is not a grade you can keep until the breaker frees it.

struct Assay {
    int order[N_PHASE];
    int n = 0;
    bool seen(int p) const { for (int i = 0; i < n; ++i) if (order[i] == p) return true; return false; }
};

// Grow the assay as new minerals appear with depth; never re-sort what is already
// on screen, only append. The order you struck the seams is information too.
static void assay_update(Assay& a, const Substance& pile) {
    for (int p = 0; p < N_PHASE; ++p)
        if (!a.seen(p) && pile.phase_mass(p) > SPECK) a.order[a.n++] = p;
}

// Mineral phases that are ore, i.e. not the quartz country rock. Used only to
// decide whether this pile mixes an oxide and a sulfide -- the two-fires problem.
static bool is_sulfide(int p) { return p == CHALCOCITE; }
static bool is_oxide_ore(int p) { return p == CUPRITE || p == CASSITERITE; }

static const int PANEL_COL = 40;

static void draw_assay(const Substance& pile, const Assay& a) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };
    const double m = pile.total_mass();

    line("what the spoil pile holds");
    line("%-11s %7s %7s", "", "grams", "grade");
    line("%s", "--------------------------------");
    for (int i = 0; i < a.n; ++i) {
        const int p = a.order[i];
        const double mp = pile.phase_mass(p);
        if (mp < SPECK) continue;
        line("%-11s %7.1f %6.1f%%", PHASES[p].id, mp * 1000.0,
             m > 1e-9 ? 100.0 * mp / m : 0.0);
    }
    line("%s", "--------------------------------");
    line("%-11s %7.1f", "all rock", m * 1000.0);

    // The whole honest point: this is locked. A grade you can see, not keep.
    double ore = 0.0;
    bool oxide = false, sulfide = false;
    for (int p = 0; p < N_PHASE; ++p) {
        if (p == QUARTZ) continue;
        const double mp = pile.phase_mass(p);
        if (mp < SPECK) continue;
        ore += mp;
        if (is_oxide_ore(p)) oxide = true;
        if (is_sulfide(p))   sulfide = true;
    }
    if (ore * 1000.0 > 0.05) {
        line("  %.1f g of it is ore, and all of it", ore * 1000.0);
        line("  is locked -- rock until you crush it.");
    } else {
        line("  nothing in it but country rock.");
    }
    if (oxide && sulfide)
        line("  it mixes oxide and sulfide: two fires.");
}

// ---------------------------------------------------------------------------
// The dig bar. Vertical, one band per tier, shallow at the top. The pit edge is a
// marker that descends as you strike; the band it sits in is the ground you are
// cutting right now, and it is lit. This is the persistent display, and it is made
// of ground rather than numbers -- the same choice the pan made with its section
// of sand.

static const char* AMBIENT[] = {
    "a crow watches from the one dead tree and says nothing",
    "the wind comes up the valley and dies again",
    "your palms have blistered and then gone hard",
    "the sun has moved; the shadow of the spoil heap is longer",
    "somewhere below the rock, water is running you cannot reach",
    "the shovel rings differently down here -- harder ground",
    "grit has worked its way into everything you own",
    "a long way off, the smoke of somebody else's fire",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(Place at, double depth, const Substance& pile, const Assay& assay,
                 bool shovel, double t, const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-44s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);

    std::printf("   you are standing at  x=%.0f m  y=%.0f m\n", at.x, at.y);
    std::printf("   in your hands: %s\n\n", shovel ? "a shovel" : "your two hands");

    // The bar. Each tier is a band three rows tall; the pit edge is '>'.
    const int here = tier_at(depth);
    const double frac = depth - std::floor(depth);        // how far into `here`
    std::printf("   the cut, top to bottom:\n\n");
    for (int tband = 0; tband < N_TIER; ++tband) {
        const bool lit = (tband == here);
        for (int r = 0; r < 3; ++r) {
            // The pit edge sits at `frac` of the way down the active band.
            const bool edge = lit && r == (int)std::floor(frac * 3.0);
            const char* face = tier_face(tband);
            std::printf("      %c%c| %c%c%c%c%c |%c %s\n",
                edge ? '=' : ' ', edge ? '=' : ' ',
                lit ? '#' : (tband < here ? ':' : '.'),
                lit ? '#' : (tband < here ? ':' : '.'),
                lit ? '#' : (tband < here ? ':' : '.'),
                lit ? '#' : (tband < here ? ':' : '.'),
                lit ? '#' : (tband < here ? ':' : '.'),
                edge ? '<' : ' ',
                r == 1 ? face : "");
        }
    }
    if (depth >= MAX_DEPTH - 1e-9)
        std::printf("\n   the shovel is on bedrock. There is nothing deeper here.\n");
    else
        std::printf("\n   depth: %.1f of %d layers\n", depth, N_TIER);

    std::printf("\n   %s\n", nag ? nag : "");
    std::printf("\n   [space] strike   [t] hand/shovel   [w/a/s/d] walk"
                "   [k] shoulder it and go   [q] quit\n");

    draw_assay(pile, assay);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "dig: needs a terminal. Run it yourself: cd core && make dig\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You are standing in a valley with a shovel you made and a bag to fill.\n\n"
        "   Somewhere under this ground is copper, and a long walk east of it, tin.\n"
        "   Most of the valley is barren rock and will read as nothing. You find ore\n"
        "   by walking until the ground under you changes.\n\n"
        "   Digging is not one press. You strike, and strike again, and the pit eats\n"
        "   down through the layers -- and the layers are not the same. What is near\n"
        "   the top is not what is at the bottom. How deep to go is the only thing you\n"
        "   decide, and nobody is going to tell you what it is worth.\n\n"
        "   Everything you dig comes up as rock. You cannot separate rock. That is a\n"
        "   job for the breaking station, and this is not it.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // Start inside the copper body but off its center, so the very first strike
    // reads something and walking still changes it -- prospecting has to have a
    // gradient to climb. copper-hill is at (0,0), radius 40 (geology.h).
    Place at { 22.0, 0.0 };
    double depth = 0.0, t = 0.0;
    bool shovel = false;
    Substance pile;
    Assay assay;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "You set the point of the shovel against the ground.";
    double nag_t = 6;

    bool running = true, quit = false, hauled = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 't') {
                shovel = !shovel;
                nag = shovel ? "You take up the shovel. It bites deep and heavy."
                             : "You set the shovel down and work with your hands.";
                nag_t = t + 3;
            }
            else if (c == ' ') {
                const double step = shovel ? SHOVEL_STEP : HAND_STEP;
                const double bite = shovel ? SHOVEL_BITE : HAND_BITE;
                if (depth >= MAX_DEPTH - 1e-9) {
                    nag = "Bedrock. The point skips off it. Move, or take what you have.";
                    nag_t = t + 3;
                } else {
                    const int before = tier_at(depth);
                    // Take this strike's bite from the tier the pit edge is in.
                    pile.add(sample(at, tier_at(depth), bite));
                    depth = std::fmin(MAX_DEPTH, depth + step);
                    assay_update(assay, pile);
                    t += shovel ? 4.0 : 1.5;   // a shovel-stroke is slower than a scratch
                    const int after = tier_at(depth);
                    if (after != before && t > nag_t) {
                        nag = after == MIDDLE
                            ? "The rust runs out. You are into pale, leached ground."
                          : after == DEEP
                            ? "The rock goes hard and fresh. This is the deep ore, if there is any."
                            : "The face has changed.";
                        nag_t = t + 4;
                    }
                }
            }
            else if (c == 'w' || c == 'a' || c == 's' || c == 'd') {
                if (c == 'w') at.y += WALK_STEP;
                if (c == 's') at.y -= WALK_STEP;
                if (c == 'd') at.x += WALK_STEP;
                if (c == 'a') at.x -= WALK_STEP;
                // A new spot is a new hole. You carry the spoil you already dug;
                // you do not carry the pit.
                depth = 0.0;
                t += 8.0; // walking ten meters over rough ground costs a little
                nag = "You walk to fresh ground and start a new hole at the surface.";
                nag_t = t + 3;
            }
            else if (c == 'k') {
                if (pile.total_mass() < SPECK) {
                    nag = "You have dug nothing. There is nothing to carry.";
                    nag_t = t + 3;
                } else { running = false; hauled = true; }
            }
        }

        t += 0.05;
        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 41) % N_AMBIENT]; ambient_t = t + 41; }
        if (t > nag_t) nag.clear();

        draw(at, depth, pile, assay, shovel, t, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(0.05);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !hauled || pile.total_mass() < SPECK) {
        std::printf("   You drop the shovel and walk away with an empty bag.\n\n");
        return 0;
    }

    // The truthful ending. Only what a scale and an eye can tell him -- and the one
    // thing the ground has been hiding all along: it is all still rock.
    const double m = pile.total_mass();
    double ore = 0.0;
    bool oxide = false, sulfide = false, tin = false;
    for (int p = 0; p < N_PHASE; ++p) {
        if (p == QUARTZ) continue;
        const double mp = pile.phase_mass(p);
        if (mp < SPECK) continue;
        ore += mp;
        if (is_oxide_ore(p)) oxide = true;
        if (is_sulfide(p))   sulfide = true;
        if (p == CASSITERITE) tin = true;
    }

    std::printf("   You shoulder the bag. It is heavy for what is in it.\n\n");
    std::printf("   You are carrying %.0f g of rock.\n", m * 1000.0);
    if (ore * 1000.0 < 0.05) {
        std::printf("   None of it is ore. You dug barren country rock.\n");
        std::printf("   The panel never twitched because there was nothing under you.\n\n");
        return 0;
    }
    std::printf("   Of that, %.0f g is ore -- a grade of %.0f%%.\n", ore * 1000.0, 100.0 * ore / m);
    std::printf("   But it is locked in the rock. You cannot smelt it and you cannot\n");
    std::printf("   even sort it yet. It has to go to the breaking station and be\n");
    std::printf("   crushed before a single grain of it is free.\n\n");
    if (oxide && sulfide) {
        std::printf("   You dug deep, and the pile is mixed: an oxide from the cap and a\n");
        std::printf("   sulfide from the root. They do not take the same fire. Once it is\n");
        std::printf("   crushed you will have to cob them apart by eye -- the sulfide has\n");
        std::printf("   to be roasted before any furnace will look at it.\n\n");
    } else if (oxide && !sulfide) {
        std::printf("   You stayed shallow. The pile is clean oxide -- one ore, one fire.\n");
        std::printf("   Smaller, but it will not fight you.\n\n");
    }
    if (tin)
        std::printf("   It is tin, not copper. There is no copper in this bag, and no bag\n"
                    "   has ever held both -- they do not grow in the same ground.\n\n");
    std::printf("   Dig shallow for a little clean ore; dig deep for more of it and a\n");
    std::printf("   harder day at the furnace. That was the only choice out here.\n\n");
    std::printf("   It cost you %d minutes and %d seconds.\n\n", (int)t / 60, (int)t % 60);
    return 0;
}
