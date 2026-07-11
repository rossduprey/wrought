// Phase A, the fifth playable slice: haul the heap.
//
// pan.cpp asked whether a wash is a game; dig.cpp, whether a dig is; breaker.cpp,
// whether freeing the ore is; furnace.cpp, whether firing it is. This file takes
// the step the other four leave out -- the walk between the hole and the breaking
// stone -- and asks whether CARRYING is a game, or a cutscene between the dig and
// the crush. It is the last unbuilt piece of the front half of the chain.
//
// It runs on no sim header of its own. The furnace stood on smelt.h's tabulated
// fires; the carry has no physics to honor but geometry -- the one thing it must
// not invent is the DISTANCE, and it does not: the valley's deposits sit at real
// coordinates in geology.h, and the walk is measured off them. Everything else
// here (capacities, walk speed, the cost of building a cart) is authored pacing,
// the carry's counterpart to the pan's STROKE and the furnace's BELLOWS_PUMP, and
// no test or finding reads it.
//
// -- The finding this slice is built on. -----------------------------------
//
// geology.h's dig hands you LOCKED rock: the ore is disseminated, glued to gangue
// you cannot tell it apart from until the breaker crushes it. So the spoil pile at
// the hole is ONE uniform grade -- and a probe (2026-07-11) confirms it is flat: a
// shovel column off the copper hill reads 30% ore by mass no matter the bite. That
// is the whole trap. You cannot high-grade the pile at the hole, because you cannot
// SEE the ore in it; every kilogram of "waste" you leave behind is exactly its
// grade in copper left behind with it. The heavy, mostly-worthless-looking rock is
// not worthless -- it is un-sorted -- so the entire pile is the haul, waste and all.
//
// And the pile is bigger than your hands. The same probe on the numbers this slice
// authors:
//
//   heap from a shovel column:              12 kg, 30% ore (3.6 kg of copper mineral)
//   HAND-haul it 80 m to the breaker:        6 trips,  ~14 min of walking
//   BACK-load (a shouldered sack):           3 trips,  ~7 min
//   CART (built once, up front):             1 trip,   ~2 min
//
// The shovel multiplies what you pull OUT of the ground tenfold; your hands do not
// grow to match. So a shovel with no cart just buries you in rock you cannot carry,
// and the cart is the rung that unburies you. It is also what makes DISTANCE
// affordable: the breaker is 80 m off, but the tin ground is 323 m off (that number
// is real -- sqrt(300^2+120^2) straight out of geology.h's DEPOSITS), and by hand
// that is an hour of walking per heap. The cart is what turns a far, rich deposit
// from a fantasy into a decision. None of this is stated to the player -- it is
// felt, one trip at a time, and only named at the end as the cost it already was.
//
// -- The gesture. -----------------------------------------------------------
//
// The other slices each have one gesture hammered many times (swirl, strike, blow,
// bellows). The carry's is the TRIP: you load what you can and you WALK it, and you
// do it again, and again, watching the heap at the hole shrink and the pile at the
// breaker grow. A terminal cannot see you hold a walk key any more than it could see
// the pan's held swirl, so a trip is an impulse -- one key, one round trip, the walk
// played out so you feel the road. The rungs (a shouldered sack, then a cart you
// stop and build) change how much moves per trip, and the only decision the slice
// withholds is the one it is here to teach: nobody tells you the pile is too big for
// your hands, or that the cart would pay for itself. You find out by hauling. The
// display is the two piles and the road between them -- the material, never a
// projected trip count -- and the cost lands, as always, only at the end.
//
// Build: make carry   (needs a terminal; it reads single keypresses)

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
// The haul's pace. AUTHORED, the carry's counterpart to the furnace's BELLOWS_PUMP:
// rates that set how the work FEELS and that no test reads. The DISTANCES they play
// against are NOT authored here -- the breaker sits at an authored placeholder
// offset (#28, like the deposit centers), but the deposit-to-deposit geometry the
// report leans on is geology.h's own DEPOSITS coordinates.
static constexpr double SHOVEL_BITE = 4.0;    // kg/tier a full-depth shovel dig takes. AUTHORED.
static constexpr double D_BREAKER   = 80.0;   // m from this hole to the breaking stone. AUTHORED (#28).
static constexpr double WALK        = 1.1;    // m/s under load. AUTHORED.

static constexpr double CAP_HANDS = 2.0;      // kg your two hands carry in one trip. AUTHORED.
static constexpr double CAP_BACK  = 5.0;      // kg a shouldered sack carries. AUTHORED.
static constexpr double CAP_CART  = 20.0;     // kg a cart carries. AUTHORED.

static constexpr double BACK_COST = 30.0;     // in-world s to shoulder a sack. AUTHORED.
static constexpr double CART_COST = 180.0;    // in-world s to lash a cart together. AUTHORED.

static constexpr int    LEG_FRAMES = 28;      // ticks to animate one leg of a trip. AUTHORED feel.

// A grain has to weigh something before the panel admits it exists.
static constexpr double SPECK = 1e-6;

static bool is_copper_ore(int p) { return p == CUPRITE || p == CHALCOCITE; }

static double ore_mass(const Substance& s) {
    double m = 0.0;
    for (int p = 0; p < N_PHASE; ++p) if (is_copper_ore(p)) m += s.phase_mass(p);
    return m;
}

enum Carrier { HANDS, BACK, CART };
static double capacity(Carrier c) {
    return c == CART ? CAP_CART : c == BACK ? CAP_BACK : CAP_HANDS;
}
static const char* carrier_name(Carrier c) {
    return c == CART ? "a cart" : c == BACK ? "a shouldered sack" : "your two hands";
}

// ---------------------------------------------------------------------------
// Terminal. Lifted from furnace.cpp (which lifted it from breaker.cpp, from dig.cpp,
// from pan.cpp) -- a raw tty with a non-blocking read is the one piece of machinery
// every slice needs and none should reinvent.

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
// The road, drawn as itself: the hole on the left, the breaking stone on the right,
// and you somewhere on it when you are walking. `where` is 0 at the hole, 1 at the
// breaker; `loaded` says whether you are trudging out under the rock or coming back
// empty. This is the material the slice is about -- the distance -- shown as ground
// you cross, not as a number of meters remaining.
static void draw_road(int row, double where, bool walking, bool loaded) {
    const int W = 28;
    char road[64];
    for (int i = 0; i < W; ++i) road[i] = '.';
    road[W] = '\0';
    std::printf("\033[%d;4Hhole |%s| stone\n", row, road);
    if (walking) {
        int pos = (int)std::lround(where * (W - 1));
        if (pos < 0) pos = 0;
        if (pos > W - 1) pos = W - 1;
        std::printf("\033[%d;%dH%s", row, 10 + pos, loaded ? "@" : "o");
    }
}

// A pile, drawn as a heap of broken ground -- a bar of the rock itself scaled to how
// much of the original heap it holds. You cannot see the ore in it (that is the
// whole finding), so the panel shows only rock: heavy, coarse, un-sorted.
static void draw_pile(int row, int col, const char* label, double mass, double of) {
    const int W = 18;
    char b[32];
    const double frac = of > SPECK ? mass / of : 0.0;
    const int fill = (int)std::lround((frac < 0 ? 0 : frac > 1 ? 1 : frac) * W);
    for (int i = 0; i < W; ++i) b[i] = i < fill ? '#' : ' ';
    b[W] = '\0';
    std::printf("\033[%d;%dH%-14s |%s| %4.0f kg", row, col, label, b, mass);
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the rock grinds and shifts on your back; a corner digs into your spine",
    "your boots find the same ruts they wore on the last trip",
    "the heap looks no smaller from here than it did an hour ago",
    "you shift the load to the other shoulder and keep walking",
    "sweat stings your eyes; the stone never seems to get closer",
    "the empty walk back is the only rest there is, and it is not much",
    "dust cakes in the creases of your hands where the rock rode",
    "your knees remember every one of these trips",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(double heap, double delivered, double of, Carrier carrier,
                 double load, int trips, double t, bool walking, double where,
                 bool loaded, const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "",
                (int)t / 60, (int)t % 60);

    std::printf("   the spoil heap at the hole, and the road to the breaking stone\n\n");
    std::printf("   trips to the stone: %d\n\n", trips);

    draw_pile(6, 4, "at the hole", heap, of);
    draw_pile(7, 4, "at the stone", delivered, of);

    draw_road(9, where, walking, loaded);

    // The carrier, and what rides on it this trip.
    std::printf("\033[11;4Hyou carry it in %s (up to %.0f kg a trip)",
                carrier_name(carrier), capacity(carrier));
    if (walking)
        std::printf("\033[12;4Hthis trip: %.0f kg on your back", load);
    else
        std::printf("\033[12;4H%s", "");

    std::printf("\033[14;4H%s\n", nag ? nag : "");

    std::printf("\033[16;4H[g] load up and haul a trip to the stone\n");
    std::printf("\033[17;4H[b] shoulder a sack   [c] build a cart\n");
    std::printf("\033[18;4H[q] set the load down and stop\n");
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// The reckoning. Only what you could know standing between the two piles: how much
// rock you moved, how much you left at the hole, how many trips it took, how long.
// And then the two things the walking taught without saying: the rock you left is
// ORE you left (the pile was one grade, so there was no waste to leave, only un-
// sorted ore), and what the cart was worth -- named only now, as the cost it was.
static void report(double heap_left, double delivered, double grade,
                   Carrier carrier, int trips, double t) {
    std::printf("   You set the load down and straighten your back.\n\n");

    if (delivered * 1.0 < SPECK) {
        std::printf("   You carried nothing to the stone. The whole heap is still at the\n"
                    "   hole, and every grain of ore in it with the rock.\n\n");
        return;
    }

    std::printf("   You carried %.0f kg of broken ground to the breaking stone in %d %s,\n",
                delivered, trips, trips == 1 ? "trip" : "trips");
    std::printf("   across %d minutes and %d seconds of walking.\n\n",
                (int)t / 60, (int)t % 60);

    if (heap_left * 1.0 > 0.5) {
        // The finding, as consequence: uniform grade means left rock is left ore.
        const double ore_left = grade * heap_left;
        std::printf("   %.0f kg is still at the hole. You could not tell its ore from its\n"
                    "   waste -- nothing is freed until the stone crushes it -- so the heap\n"
                    "   was one grade throughout, and the rock you walked away from carried\n"
                    "   %.0f g of copper away with it. Not waste left behind: ore left behind,\n"
                    "   still un-sorted, back at the hole where no fire will ever find it.\n\n",
                    heap_left, ore_left * 1000.0);
    } else {
        std::printf("   The hole is bare -- you moved the whole heap. Every grain the dig\n"
                    "   turned up is at the stone now, waiting on the hammer.\n\n");
    }

    // What the carrier cost or saved, named only now. The distance to the far
    // deposit is real geology, not a figure of speech.
    const double gap = std::sqrt(
        (DEPOSITS[0].cx - DEPOSITS[1].cx) * (DEPOSITS[0].cx - DEPOSITS[1].cx) +
        (DEPOSITS[0].cy - DEPOSITS[1].cy) * (DEPOSITS[0].cy - DEPOSITS[1].cy));

    if (carrier == HANDS && trips >= 4) {
        std::printf("   You did the whole haul in your two hands, a few kilograms a trip.\n"
                    "   A cart carries ten times that -- what took you %d trips would have\n"
                    "   been one or two. The shovel filled the heap faster than your hands\n"
                    "   could empty it; that is the shape of it, and the cart is the fix.\n\n",
                    trips);
    } else if (carrier == CART) {
        std::printf("   The cart earned back the time you spent building it and then some --\n"
                    "   it swallowed in a load or two what your hands could not have matched\n"
                    "   in an afternoon of trips.\n\n");
    }

    std::printf("   This stone is %.0f m off. The tin ground is %.0f m -- four times the\n"
                "   walk, and by hand that is the better part of an hour a heap. Far, rich\n"
                "   ground is only worth turning over once you have a cart to carry it.\n\n",
                D_BREAKER, gap);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "carry: needs a terminal. Run it yourself: cd core && make carry\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You have dug a full-depth column at the copper hill: a heap of broken\n"
        "   ground, oxide and sulfide and barren rock all heaped together. It is\n"
        "   locked rock -- nothing in it is freed yet, so you cannot tell the ore\n"
        "   from the waste by looking. The whole heap has to go to the breaking\n"
        "   stone before any of it is worth a thing.\n\n"
        "   The stone is a walk off. You carry the rock there yourself, a load at a\n"
        "   time, and haul back for more. Your two hands do not hold much. There is\n"
        "   a sack you can shoulder, and a cart you can stop and build, if you judge\n"
        "   the heap worth the trouble -- but nobody is going to tell you whether it\n"
        "   is. Load up, walk it over, and find out what the haul costs.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // The heap: a shovel full-depth dig at the copper hill center. Materialized the
    // way every slice materializes its feed -- so this one stands alone but is fed
    // exactly what the dig produces: locked, mixed, one uniform grade.
    Substance heap = dig_column(Place{0.0, 0.0}, SHOVEL_BITE);
    const double heap0 = heap.total_mass();
    const double ore0  = ore_mass(heap);
    const double grade = heap0 > SPECK ? ore0 / heap0 : 0.0;  // uniform: the finding
    double heap_mass = heap0;
    double delivered = 0.0;

    Carrier carrier = HANDS;
    int trips = 0;
    double t = 0.0;

    // Walk animation state. A trip is: out (loaded), then back (empty).
    bool walking = false, outbound = true;
    int leg = 0;
    double load = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "The heap is at your feet. Load up and start walking.";
    double nag_t = 6;

    bool running = true, quit = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (walking) continue;   // you cannot re-tool or re-load mid-stride
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'g') {
                if (heap_mass < 0.5) {
                    nag = "The hole is bare -- there is nothing left to carry.";
                    nag_t = t + 4;
                } else {
                    load = capacity(carrier);
                    if (load > heap_mass) load = heap_mass;
                    heap_mass -= load;
                    walking = true; outbound = true; leg = 0;
                }
            }
            else if (c == 'b') {
                if (carrier == HANDS) {
                    carrier = BACK;
                    t += BACK_COST;
                    nag = "You knot a sack and swing it up onto your shoulder. It holds "
                          "more than your hands, and rides worse.";
                    nag_t = t + 5;
                } else {
                    nag = "You already carry it on your back.";
                    nag_t = t + 3;
                }
            }
            else if (c == 'c') {
                if (carrier != CART) {
                    carrier = CART;
                    t += CART_COST;
                    nag = "You stop and lash a cart together -- poles, a bed, a pair of "
                          "wheels. It costs you the time now. It carries ten hands' worth.";
                    nag_t = t + 6;
                } else {
                    nag = "The cart is already built.";
                    nag_t = t + 3;
                }
            }
        }

        // Advance a walk in progress. Each leg animates over LEG_FRAMES ticks; the
        // in-world clock only takes the trip's real cost (round trip at WALK) when
        // the whole trip lands, so the animation is feel and the clock is honest.
        if (walking) {
            ++leg;
            if (leg >= LEG_FRAMES) {
                leg = 0;
                if (outbound) {
                    outbound = false;                 // arrived; turn for home
                    delivered += load;
                } else {
                    walking = false;                  // back at the hole
                    ++trips;
                    t += 2.0 * D_BREAKER / WALK;       // the trip's true cost, in-world
                    load = 0.0;
                    if (heap_mass < 0.5) {
                        nag = "That was the last of it -- the hole is bare.";
                        nag_t = t + 5;
                    }
                }
            }
        }

        const double where = outbound ? (double)leg / LEG_FRAMES
                                      : 1.0 - (double)leg / LEG_FRAMES;

        t += 0.05;
        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 37) % N_AMBIENT]; ambient_t = t + 37; }
        if (t > nag_t) nag.clear();

        draw(heap_mass, delivered, heap0, carrier, load, trips, t, walking, where,
             outbound, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(0.05);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit && delivered < SPECK) {
        std::printf("   You walk away from the heap without carrying a stone of it. The ore\n"
                    "   is still in the ground you turned over, which is the same as still in\n"
                    "   the ground.\n\n");
        return 0;
    }

    report(heap_mass, delivered, grade, carrier, trips, t);
    return 0;
}
