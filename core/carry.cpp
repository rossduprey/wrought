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
// here (capacities, walk speed, the cost of building a cart, of a wash) is authored
// pacing, the carry's counterpart to the pan's STROKE and the furnace's BELLOWS_
// PUMP, and no test or finding reads it.
//
// -- The finding this slice is built on. -----------------------------------
//
// CORRECTED 2026-07-11, from monitored play: an earlier version of this slice
// stood on a falsehood -- that the dig hands you ONE uniform grade of locked rock,
// so the whole heap must go to the breaker and every kilogram left is ore left. It
// does not. geology.h now breaks the pick's spoil across a size DISTRIBUTION, and
// liberation rides on it: the FINE end of the heap is already-free grains a pan can
// take at the hole with no breaker at all, and the coarse end is locked rock the
// breaker is the only way into. How much frees at the face is the mineralogy's to
// decide, not ours (sourced in geology.h) -- and at THIS hole, the copper hill, it
// decides against you. A probe on the corrected model:
//
//   heap from a pick column (copper hill):  12 kg, 30% ore (3.6 kg copper mineral)
//   of that, FREE at the face (washable):   ~0.1 kg copper -- the oxide's fine tail
//   LOCKED in the coarse (breaker only):    ~3.5 kg copper -- nearly all of it
//
// So the wash at the hole is real but nearly worthless HERE: this is hard rock, the
// copper is ground into the gangue, and the stone is the only way to it. That is
// the lesson, and it is the true reason hard-rock ore built stamp mills. The knife
// turns at the tin ground: there the ore is placer cassiterite, coarse and weathered
// free, and the same wash would take almost ALL of it at the hole with no stone
// touched -- which is why placer tin was panned by a single hand. The slice teaches
// the tool (a wash, a haul) and lets the ground teach which one this ground rewards.
//
// And the locked coarse is bigger than your hands. The numbers this slice authors:
//
//   HAND-haul the coarse 80 m to the breaker:  6 trips,  ~14 min of walking
//   BACK-load (a shouldered sack):             3 trips,  ~7 min
//   CART (built once, up front):               1 trip,   ~2 min
//
// The pick multiplies what you win from the ground tenfold; your hands do not grow
// to match. So a pick with no cart just buries you in rock you cannot carry, and the
// cart is the rung that unburies you. It is also what makes DISTANCE affordable: the
// breaker is 80 m off, but the tin ground is 323 m off (real -- sqrt(300^2+120^2)
// straight out of geology.h's DEPOSITS), and by hand that is an hour a heap. None of
// this is stated to the player -- it is felt, one trip at a time, and named only at
// the end as the cost it already was.
//
// -- The gestures. ----------------------------------------------------------
//
// Two, where the other slices had one. The WASH is the tin-or-toll question asked
// at the hole: screen the fines off the heap and pan them where you stand, winning
// whatever came up free -- everything, at a placer; a sliver, at this hard hill.
// The TRIP is the haul of what the wash could not free: you load the locked coarse
// and you WALK it, and again, watching the heap shrink and the pile at the breaker
// grow. A terminal cannot see you hold a walk key, so a trip is an impulse -- one
// key, one round trip, the walk played out so you feel the road. The rungs (a
// shouldered sack, then a cart you stop and build) change how much moves per trip.
// Nobody tells you the wash is futile here, or that the cart would pay for itself;
// you find out by doing. The display is the two piles and the road between them --
// the material, never a projected count -- and the cost lands only at the end.
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
#include "separate.h"   // screen() + separate(): the wash at the hole is the real pan

using namespace wrought;

// ---------------------------------------------------------------------------
// The haul's pace. AUTHORED, the carry's counterpart to the furnace's BELLOWS_PUMP:
// rates that set how the work FEELS and that no test reads. The DISTANCES they play
// against are NOT authored here -- the breaker sits at an authored placeholder
// offset (#28, like the deposit centers), but the deposit-to-deposit geometry the
// report leans on is geology.h's own DEPOSITS coordinates.
static constexpr double PICK_BITE = 4.0;      // kg/tier a full-depth pick dig wins. AUTHORED.
static constexpr double D_BREAKER = 80.0;     // m from this hole to the breaking stone. AUTHORED (#28).
static constexpr double WALK      = 1.1;      // m/s under load. AUTHORED.

static constexpr double CAP_HANDS = 2.0;      // kg your two hands carry in one trip. AUTHORED.
static constexpr double CAP_BACK  = 5.0;      // kg a shouldered sack carries. AUTHORED.
static constexpr double CAP_CART  = 20.0;     // kg a cart carries. AUTHORED.

static constexpr double BACK_COST = 30.0;     // in-world s to shoulder a sack. AUTHORED.
static constexpr double CART_COST = 180.0;    // in-world s to lash a cart together. AUTHORED.
static constexpr double WASH_COST = 90.0;     // in-world s to screen and pan the fines. AUTHORED.

static constexpr int    LEG_FRAMES = 28;      // ticks to animate one leg of a trip. AUTHORED feel.

// A grain has to weigh something before the panel admits it exists.
static constexpr double SPECK = 1e-6;

static bool is_copper_ore(int p) { return p == CUPRITE || p == CHALCOCITE; }

static double ore_mass(const Substance& s) {
    double m = 0.0;
    for (int p = 0; p < N_PHASE; ++p) if (is_copper_ore(p)) m += s.phase_mass(p);
    return m;
}

// Take a uniform scoop off the heap: hauling a load removes rock in the proportion
// it sits in the pile (you cannot high-grade a shovel of locked rock -- only a wash
// can part it, and only what came up free). Keeps the Substance and its mass in step
// so a later wash reads the true remainder.
static void scale(Substance& s, double k) {
    for (int p = 0; p < N_PHASE; ++p)
        for (int z = 0; z < N_SIZE; ++z) { s.freegrain[p][z] *= k; s.composite[p][z] *= k; }
}

enum Carrier { BY_HANDS, BY_BACK, BY_CART };  // BY_* to avoid separate.h's HANDS separator
static double capacity(Carrier c) {
    return c == BY_CART ? CAP_CART : c == BY_BACK ? CAP_BACK : CAP_HANDS;
}
static const char* carrier_name(Carrier c) {
    return c == BY_CART ? "a cart" : c == BY_BACK ? "a shouldered sack" : "your two hands";
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
// much of the original heap it holds. The panel shows only rock: heavy, coarse, and
// mostly locked -- what the wash could not free and the breaker still must.
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
                 double load, int trips, double t, double won_here, bool walking,
                 double where, bool loaded, const char* ambient, const char* nag) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "",
                (int)t / 60, (int)t % 60);

    std::printf("   the spoil heap at the hole, and the road to the breaking stone\n\n");
    std::printf("   trips to the stone: %-3d   washed free at the hole: %.0f g copper\n\n",
                trips, won_here * 1000.0);

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

    std::printf("\033[16;4H[w] wash the loose fines here   [g] load up and haul a trip\n");
    std::printf("\033[17;4H[b] shoulder a sack   [c] build a cart\n");
    std::printf("\033[18;4H[q] set the load down and stop\n");
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// The reckoning. Only what you could know standing between the two piles: what the
// wash won you here, how much locked rock you hauled, how much you left, how long.
// And then the things the work taught without saying: at this hard hill almost none
// of the copper would come free at the face, so the coarse you left is LOCKED ore
// the breaker never got to; what the cart was worth; and the tin ground, where the
// same wash would have taken it all. Named only now, as the cost it already was.
static void report(double heap_left, double delivered, double won_here, double grade,
                   Carrier carrier, int trips, double t) {
    std::printf("   You set the load down and straighten your back.\n\n");

    if (won_here > SPECK)
        std::printf("   Washing the loose fines at the hole won you %.0f g of copper without a\n"
                    "   single step -- the little of this ore that came up already free.\n\n",
                    won_here * 1000.0);

    if (delivered * 1.0 < SPECK) {
        std::printf("   You carried nothing to the stone. The locked rock -- nearly all of the\n"
                    "   copper -- is still at the hole, and no wash will free it. Only the\n"
                    "   breaker can, and it is a walk away.\n\n");
    } else {
        std::printf("   You carried %.0f kg of locked rock to the breaking stone in %d %s,\n",
                    delivered, trips, trips == 1 ? "trip" : "trips");
        std::printf("   across %d minutes and %d seconds of walking.\n\n",
                    (int)t / 60, (int)t % 60);
    }

    if (heap_left * 1.0 > 0.5) {
        // The finding, as consequence: what you left is coarse LOCKED ore -- not
        // free, so the wash could not save it, and it never reached the stone.
        const double ore_left = grade * heap_left;
        std::printf("   %.0f kg is still at the hole -- coarse, locked rock. The wash could not\n"
                    "   part it (the ore is ground into the gangue) and it never reached the\n"
                    "   stone, so the roughly %.0f g of copper in it is ore left behind: not\n"
                    "   waste, un-freed ore, back where no fire will ever find it.\n\n",
                    heap_left, ore_left * 1000.0);
    } else if (delivered >= SPECK) {
        std::printf("   The hole is bare -- the locked coarse is all at the stone now, waiting\n"
                    "   on the hammer that is the only way into it.\n\n");
    }

    // What the carrier cost or saved, named only now. The distance to the far
    // deposit is real geology, not a figure of speech.
    const double gap = std::sqrt(
        (DEPOSITS[0].cx - DEPOSITS[1].cx) * (DEPOSITS[0].cx - DEPOSITS[1].cx) +
        (DEPOSITS[0].cy - DEPOSITS[1].cy) * (DEPOSITS[0].cy - DEPOSITS[1].cy));

    if (carrier == BY_HANDS && trips >= 4) {
        std::printf("   You hauled the locked coarse in your two hands, a few kilograms a trip.\n"
                    "   A cart carries ten times that -- what took you %d trips would have\n"
                    "   been one or two. The pick filled the heap faster than your hands could\n"
                    "   empty it; that is the shape of it, and the cart is the fix.\n\n",
                    trips);
    } else if (carrier == BY_CART) {
        std::printf("   The cart earned back the time you spent building it and then some --\n"
                    "   it swallowed in a load or two what your hands could not have matched\n"
                    "   in an afternoon of trips.\n\n");
    }

    std::printf("   This is hard rock: the wash at the hole barely helped, and the stone was\n"
                "   the only way in. It would not be so at the tin ground, %.0f m off (this\n"
                "   stone is %.0f m) -- placer tin comes up coarse and free, and there the\n"
                "   wash you just did would have taken nearly all of it, no stone touched.\n"
                "   Which ground rewards the pan and which the breaker is the ground's to say.\n\n",
                gap, D_BREAKER);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "carry: needs a terminal. Run it yourself: cd core && make carry\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You have picked a full-depth column at the copper hill: a heap of broken\n"
        "   ground, oxide and sulfide and barren rock heaped together. The pick left\n"
        "   it in every size, from dust to fist-sized stone. The loose fines you can\n"
        "   wash right here, and keep whatever came up free. The coarse rock is\n"
        "   locked -- the ore ground into it -- and only the breaking stone, a walk\n"
        "   off, can free that.\n\n"
        "   Wash the fines where you stand, or load the coarse and carry it to the\n"
        "   stone yourself, a load at a time. Your two hands do not hold much. There\n"
        "   is a sack you can shoulder, and a cart you can stop and build, if you\n"
        "   judge the heap worth it -- but nobody will tell you whether the wash is\n"
        "   worth doing here, or whether the cart pays. Try them, and find out.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    // The heap: a pick full-depth dig at the copper hill center. Materialized the
    // way every slice materializes its feed -- so this one stands alone but is fed
    // exactly what the dig produces: a size distribution, free fines over locked
    // coarse, the copper mostly in the locked part (that is the finding).
    Substance heap = dig_column(Place{0.0, 0.0}, PICK_BITE);
    const double heap0 = heap.total_mass();
    const double grade = heap0 > SPECK ? ore_mass(heap) / heap0 : 0.0; // for the ore-left estimate
    double heap_mass = heap0;
    double delivered = 0.0;
    double won_here  = 0.0;   // copper won by washing the free fines at the hole

    Carrier carrier = BY_HANDS;
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
            else if (c == 'w') {
                // Wash the loose fines where you stand: screen the sand-and-finer off
                // the coarse (cut at GRAVEL) and pan it. You keep the free ore it
                // holds; the locked coarse stays to haul. At this hard hill that ore
                // is a sliver -- but the gesture is real and the ground decides.
                if (heap_mass < 0.5) {
                    nag = "The hole is bare -- there is nothing left to wash.";
                    nag_t = t + 4;
                } else {
                    ScreenResult sc = screen(heap, 0.95, GRAVEL);      // sand-and-finer to undersize
                    SeparationResult pr = separate(sc.undersize, PAN); // pan those fines
                    // Won = only the FREE ore the pan kept. Locked composite that
                    // leaked through the screen is NOT won -- it is still locked, so
                    // it stays in the haul; a pan cannot free what the breaker must.
                    double won = 0.0;
                    for (int p = 0; p < N_PHASE; ++p)
                        if (is_copper_ore(p))
                            for (int z = 0; z < N_SIZE; ++z) won += pr.concentrate.freegrain[p][z];
                    won_here += won;
                    // The haul: the coarse oversize, plus every locked grain that
                    // rode into the pan (kept or spilled -- still locked, still ore,
                    // so neither won nor lost). Free fine gangue and any fine ore lost
                    // over the lip stay at the hole as tailings.
                    heap = sc.oversize;
                    for (int p = 0; p < N_PHASE; ++p)
                        for (int z = 0; z < N_SIZE; ++z)
                            heap.composite[p][z] += pr.concentrate.composite[p][z]
                                                  + pr.tailings.composite[p][z];
                    heap_mass = heap.total_mass();
                    t += WASH_COST;
                    nag = "You screen the fines off and pan them where you stand. The heavy "
                          "specks that stay are the ore that came up free.";
                    nag_t = t + 6;
                }
            }
            else if (c == 'g') {
                if (heap_mass < 0.5) {
                    nag = "The hole is bare -- there is nothing left to carry.";
                    nag_t = t + 4;
                } else {
                    load = capacity(carrier);
                    if (load > heap_mass) load = heap_mass;
                    scale(heap, (heap_mass - load) / heap_mass); // a uniform scoop off the pile
                    heap_mass -= load;
                    walking = true; outbound = true; leg = 0;
                }
            }
            else if (c == 'b') {
                if (carrier == BY_HANDS) {
                    carrier = BY_BACK;
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
                if (carrier != BY_CART) {
                    carrier = BY_CART;
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

        draw(heap_mass, delivered, heap0, carrier, load, trips, t, won_here, walking,
             where, outbound, ambient, nag.empty() ? nullptr : nag.c_str());
        nap(0.05);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit && delivered < SPECK && won_here < SPECK) {
        std::printf("   You walk away from the heap without washing a pan or carrying a stone\n"
                    "   of it. The ore is still in the ground you turned over, which is the\n"
                    "   same as still in the ground.\n\n");
        return 0;
    }

    report(heap_mass, delivered, won_here, grade, carrier, trips, t);
    return 0;
}
