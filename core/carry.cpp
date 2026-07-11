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
// CORRECTED 2026-07-11, from monitored play, in two passes. First: an earlier
// version stood on a falsehood -- that the dig hands you ONE uniform grade of locked
// rock, so the whole heap must go to the breaker. It does not; geology.h breaks the
// pick's spoil across a size DISTRIBUTION and liberation rides on it. Second, deeper:
// the fix at first modelled BOTH grounds as fresh-broken rock, which is right for the
// copper hill and wrong for the tin creek. A placer is not fresh rock -- it is
// alluvium a river already broke AND sorted, dropping the free heavy mineral into a
// narrow band of pannable SAND. geology.h now carries that as a Deposit's Origin, and
// this slice lets you WORK either ground and feel the difference the origin makes.
//
// Probes on the corrected model, a full-depth pick column of each (12 kg, 45% ore):
//
//   COPPER HILL (hard rock):  free at the face, washable  ~0.08 kg -- 2% of the copper
//                             locked in the coarse, breaker-only  ~nearly all of it
//   TIN CREEK   (placer):     wash (screen off cobble, pan the sand) wins ~3.2 kg tin
//                             -- 59% of it, and it LIFTS the grade 45% -> 76%, no stone
//
// So the same wash is nearly worthless at the copper hill (hard rock, the ore ground
// into the gangue, the stone the only way in -- the true reason hard-rock ore built
// stamp mills) and is the whole game at the tin creek (a placer, the tin free in the
// sand, panned where it lies -- which is why placer tin was won by a single hand).
// The slice teaches the tools (a wash, a haul) and lets the ground -- your choice of
// it -- teach which tool that ground rewards. Nobody says which; the pan's yield does.
//
// At the copper hill the locked coarse is bigger than your hands. The numbers this
// slice authors:
//
//   HAND-haul the coarse 80 m to the breaker:  6 trips,  ~14 min of walking
//   BACK-load (a shouldered sack):             3 trips,  ~7 min
//   CART (built once, up front):               1 trip,   ~2 min
//
// The pick multiplies what you win from the ground tenfold; your hands do not grow
// to match. So a pick with no cart just buries you in rock you cannot carry, and the
// cart is the rung that unburies you. It is also what makes DISTANCE affordable: the
// breaker is 80 m off, and the two grounds are 323 m apart (real -- sqrt(300^2+120^2)
// straight out of geology.h's DEPOSITS), and by hand that is an hour a heap. None of
// this is stated to the player -- it is felt, one trip at a time, and named only at
// the end as the cost it already was.
//
// -- The gestures. ----------------------------------------------------------
//
// First you CHOOSE the ground -- the copper hill or the tin creek -- and that choice
// is the fork the whole slice turns on. Then two gestures, where the other slices had
// one. The WASH is the ore-or-toll question asked at the hole: screen the coarse off
// the heap and pan the sand where you stand, winning whatever came up free -- most of
// it, at the placer; a sliver, at the hard hill. The TRIP is the haul of the coarse:
// you load it and you WALK it, and again, watching the heap shrink and the pile at the
// breaker grow -- ore worth crushing at the copper hill, barren cobble at the creek.
// A terminal cannot see you hold a walk key, so a trip is an impulse -- one key, one
// round trip, the walk played out so you feel the road. The rungs (a shouldered sack,
// then a cart you stop and build) change how much moves per trip. Nobody tells you
// the wash is futile at the hill, or the haul is futile at the creek, or the cart
// would pay for itself; you find out by doing. The display is the two piles and the
// road between them -- the material, never a projected count -- and the cost lands
// only at the end.
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

// The ore phases a dig can hand you here -- copper's two, and tin's one. A given
// ground only ever carries one family (co-location is the gate, geology.h), so
// summing all three counts exactly the ore that is actually in the heap.
static bool is_ore(int p) { return p == CUPRITE || p == CHALCOCITE || p == CASSITERITE; }

static double ore_mass(const Substance& s) {
    double m = 0.0;
    for (int p = 0; p < N_PHASE; ++p) if (is_ore(p)) m += s.phase_mass(p);
    return m;
}

// The two grounds you can work, indices into geology.h DEPOSITS. The copper hill is
// HARD ROCK (the ore locked in coarse composite, the breaker the only way in); the
// tin creek is a PLACER (the ore free in the sand, won by a wash where it lies).
enum Ground { COPPER_HILL = 0, TIN_CREEK = 1 };
static bool is_placer(Ground g) { return g == TIN_CREEK; }
static const char* metal_of(Ground g) { return g == TIN_CREEK ? "tin" : "copper"; }

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
                 double where, bool loaded, const char* ambient, const char* nag,
                 const char* metal) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "",
                (int)t / 60, (int)t % 60);

    std::printf("   the spoil heap at the hole, and the road to the breaking stone\n\n");
    std::printf("   trips to the stone: %-3d   washed free at the hole: %.0f g %s\n\n",
                trips, won_here * 1000.0, metal);

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
// wash won you here, how much rock you hauled, how much you left, how long. And then
// the things the work taught without saying -- which differ by ground, because that
// is the whole point. At the hard-rock copper hill almost none of the copper comes
// free at the face, so the coarse you left is LOCKED ore the breaker never got to,
// and the haul is the game. At the tin creek the wash already won the tin where you
// stood, so the coarse you left is BARREN cobble and the haul was for nothing. The
// slice never says which ground is which; the reckoning names what the work already
// showed. Distances are real geology, not figures of speech.
static void report(Ground ground, double heap_left, double delivered, double won_here,
                   double grade, Carrier carrier, int trips, double t) {
    const bool placer = is_placer(ground);
    const char* metal = metal_of(ground);
    std::printf("   You set the load down and straighten your back.\n\n");

    if (won_here > SPECK) {
        if (placer)
            std::printf("   Washing the sand at the hole won you %.0f g of tin without a single step\n"
                        "   -- you screened off the cobble and panned the heavy sand, and the tin\n"
                        "   was already free in it. This is placer ground: the river did the\n"
                        "   breaking, and the pan finished the job. No stone was ever needed.\n\n",
                        won_here * 1000.0);
        else
            std::printf("   Washing the loose fines at the hole won you %.0f g of copper without a\n"
                        "   single step -- the little of this ore that came up already free.\n\n",
                        won_here * 1000.0);
    }

    if (delivered * 1.0 < SPECK) {
        if (placer)
            std::printf("   You carried nothing to the stone, and you needed to carry nothing:\n"
                        "   the tin was won in the pan. The coarse left at the hole is barren\n"
                        "   cobble, not ore.\n\n");
        else
            std::printf("   You carried nothing to the stone. The locked rock -- nearly all of the\n"
                        "   copper -- is still at the hole, and no wash will free it. Only the\n"
                        "   breaker can, and it is a walk away.\n\n");
    } else {
        const bool washed = won_here > SPECK;
        std::printf("   You carried %.0f kg of rock to the breaking stone in %d %s,\n",
                    delivered, trips, trips == 1 ? "trip" : "trips");
        std::printf("   across %d minutes and %d seconds of walking%s\n\n",
                    (int)t / 60, (int)t % 60,
                    placer ? (washed ? " -- cobble the pan had already made barren."
                                     : " -- a scoop of good sand and cobble alike, unpanned.")
                           : ".");
    }

    if (heap_left * 1.0 > 0.5) {
        const double ore_left = grade * heap_left;
        if (placer && won_here > SPECK)
            std::printf("   %.0f kg is still at the hole -- coarse cobble the current could not lift\n"
                        "   with the heavies, near-barren. The roughly %.0f g of %s still in it is\n"
                        "   the coarse tail the pan let go; a stone could chase it, but the sand\n"
                        "   held the most of it and you already have that.\n\n",
                        heap_left, ore_left * 1000.0, metal);
        else if (placer)
            std::printf("   %.0f kg is still at the hole, and the tin is still in it -- roughly\n"
                        "   %.0f g, most of it in the heavy sand you never panned. A wash would\n"
                        "   have taken it where it lay; hauling the coarse to the stone chases the\n"
                        "   little that isn't there.\n\n",
                        heap_left, ore_left * 1000.0);
        else
            // The finding, as consequence: what you left is coarse LOCKED ore -- not
            // free, so the wash could not save it, and it never reached the stone.
            std::printf("   %.0f kg is still at the hole -- coarse, locked rock. The wash could not\n"
                        "   part it (the ore is ground into the gangue) and it never reached the\n"
                        "   stone, so the roughly %.0f g of copper in it is ore left behind: not\n"
                        "   waste, un-freed ore, back where no fire will ever find it.\n\n",
                        heap_left, ore_left * 1000.0);
    } else if (delivered >= SPECK && !placer) {
        std::printf("   The hole is bare -- the locked coarse is all at the stone now, waiting\n"
                    "   on the hammer that is the only way into it.\n\n");
    }

    if (carrier == BY_HANDS && trips >= 4 && !placer) {
        std::printf("   You hauled the locked coarse in your two hands, a few kilograms a trip.\n"
                    "   A cart carries ten times that -- what took you %d trips would have\n"
                    "   been one or two. The pick filled the heap faster than your hands could\n"
                    "   empty it; that is the shape of it, and the cart is the fix.\n\n",
                    trips);
    } else if (carrier == BY_CART && !placer) {
        std::printf("   The cart earned back the time you spent building it and then some --\n"
                    "   it swallowed in a load or two what your hands could not have matched\n"
                    "   in an afternoon of trips.\n\n");
    }

    // The distance between the two grounds is real: sqrt(300^2 + 120^2) = 323 m.
    const double gap = std::sqrt(
        (DEPOSITS[0].cx - DEPOSITS[1].cx) * (DEPOSITS[0].cx - DEPOSITS[1].cx) +
        (DEPOSITS[0].cy - DEPOSITS[1].cy) * (DEPOSITS[0].cy - DEPOSITS[1].cy));

    if (placer)
        std::printf("   This is placer ground, and the pan is the whole of it: the tin came up\n"
                    "   free in the sand and a wash took it where it lay. It is not so at the\n"
                    "   copper hill, %.0f m off -- there the ore is locked in hard rock, the wash\n"
                    "   wins almost nothing, and every kilogram must be hauled to the stone and\n"
                    "   crushed. One hand panned tin; the other built stamp mills for copper.\n\n",
                    gap);
    else
        std::printf("   This is hard rock: the wash at the hole barely helped, and the stone was\n"
                    "   the only way in. It would not be so at the tin creek, %.0f m off (this\n"
                    "   stone is %.0f m) -- placer tin comes up free in the sand, and there the\n"
                    "   wash you just did would have taken most of it, no stone touched.\n"
                    "   Which ground rewards the pan and which the breaker is the ground's to say.\n\n",
                    gap, D_BREAKER);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "carry: needs a terminal. Run it yourself: cd core && make carry\n");
        return 1;
    }

    raw_tty();

    // Choose the ground. This is a real fork, not a skin: the copper hill is hard
    // rock and the tin creek is a placer, and they reward opposite work. The slice
    // will not tell you which wants the pan and which wants the stone -- that is the
    // thing you are here to find out.
    std::printf("\033[H\033[J\n"
        "   Two grounds are yours to work, and they are not the same kind of ground.\n\n"
        "     [c]  the copper hill -- hard rock, an oxide cap over a sulfide root.\n"
        "     [t]  the tin creek   -- a placer, tin in the gravel of an old streambed.\n\n"
        "   Pick one and take a full-depth column of it. What comes up, and what it\n"
        "   takes to win the metal from it, is the ground's to decide -- nobody here\n"
        "   will tell you whether to reach for the pan or the breaking stone.\n\n"
        "   [c] copper hill   [t] tin creek   [q] walk away\n");
    std::fflush(stdout);
    Ground ground = COPPER_HILL;
    for (;;) {
        int c = poll_key();
        if (c == 'c') { ground = COPPER_HILL; break; }
        if (c == 't') { ground = TIN_CREEK;  break; }
        if (c == 'q') { restore_tty(); std::printf("\033[H\033[J\n   You leave both grounds unturned.\n\n"); return 0; }
        nap(0.02);
    }
    const bool placer = is_placer(ground);
    const char* metal = metal_of(ground);

    // The ground-specific brief. Same two gestures either way -- wash here, or haul
    // to the stone -- but what each is FOR differs, and the slice does not say so.
    if (placer)
        std::printf("\033[H\033[J\n"
            "   You have picked a full-depth column at the tin creek: broken streambed\n"
            "   gravel, dark heavy sand and pale cobble heaped together. There is no\n"
            "   oxide cap and no sulfide root here -- the river weathered the tin free\n"
            "   an age ago and laid it down loose in the sand. The cobble is barren.\n\n"
            "   Wash the sand where you stand and keep the heavy tin the pan holds, or\n"
            "   load the coarse and carry it to the breaking stone, a load at a time.\n"
            "   Your two hands do not hold much; there is a sack to shoulder and a cart\n"
            "   to build. Nobody will tell you whether the wash is worth doing here, or\n"
            "   whether the stone is worth the walk. Try them, and find out.\n\n"
            "   [press any key]\n");
    else
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
    while (poll_key() < 0) nap(0.02);

    // The heap: a pick full-depth dig at the chosen ground's center. Materialized the
    // way every slice materializes its feed -- so this one stands alone but is fed
    // exactly what the dig produces. At the copper hill that is a size distribution,
    // free fines over locked coarse, the copper mostly locked; at the tin creek it is
    // free heavy sand over barren cobble, the tin mostly pan-ready (that is the fork).
    Substance heap = dig_column(Place{DEPOSITS[ground].cx, DEPOSITS[ground].cy}, PICK_BITE);
    const double heap0 = heap.total_mass();
    const double grade = heap0 > SPECK ? ore_mass(heap) / heap0 : 0.0; // for the ore-left estimate
    double heap_mass = heap0;
    double delivered = 0.0;
    double won_here  = 0.0;   // ore won by washing at the hole (copper fines or tin sand)

    Carrier carrier = BY_HANDS;
    int trips = 0;
    double t = 0.0;

    // Walk animation state. A trip is: out (loaded), then back (empty).
    bool walking = false, outbound = true;
    int leg = 0;
    double load = 0.0;

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = placer
        ? "The heap is at your feet. Wash the sand, or load the coarse and walk it."
        : "The heap is at your feet. Load up and start walking.";
    double nag_t = 6;

    bool running = true, quit = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (walking) continue;   // you cannot re-tool or re-load mid-stride
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'w') {
                // Wash where you stand: screen the sand-and-finer off the coarse (cut
                // at GRAVEL) and pan it. You keep only the FREE ore the pan holds. The
                // SAME gesture at either ground -- but the ground decides what it wins:
                // at the copper hill the free ore is a sliver and the coarse is locked
                // ore to haul; at the tin creek the sand IS the ore and the coarse is
                // barren cobble. Nobody says which; the pan's yield tells you.
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
                        if (is_ore(p))
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
                    nag = placer
                        ? "You screen off the cobble and pan the heavy sand. The dark grains "
                          "that stay in the pan are tin, come up free of the rock."
                        : "You screen the fines off and pan them where you stand. The heavy "
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
             where, outbound, ambient, nag.empty() ? nullptr : nag.c_str(), metal);
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

    report(ground, heap_mass, delivered, won_here, grade, carrier, trips, t);
    return 0;
}
