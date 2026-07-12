// Phase A, the ninth playable slice: haft a stone edge to a stick, and make the
// first real tool -- the one that finally fells the tree the whole staircase has
// been leaning on.
//
// knap.cpp made an edge and stopped at a wall: a stone edge is only ever so keen.
// This verb does not try to beat that wall; it does the other thing a tool can do,
// which is put the edge on a LEVER. So this asks the question knap left open -- an
// edge is not a tool, so what is? -- and answers it with the two things haft.h says
// decide whether a bundle of head and stick is a tool at all: the lever, and the joint.
//
// It runs on haft.h -- haft() for the assembly, joint_strength()/swing_energy() for
// what a swing delivers, FELL_ENERGY for the wall -- and adds a joint you work up in
// real time and a trunk you get to swing at.
//
// -- The findings this slice is built on. ----------------------------------
//
// haft.h, and it teaches both halves. THE HAFT IS FORCE, NOT SHARPNESS: the same
// floored edge that only scraped in your fist fells a tree on the end of a handle,
// because a lever's tip moves faster the longer it is and a blow's energy grows with
// the square of that speed. The tree yields to energy, not to a razor. And THE JOINT
// IS THE CEILING: a lever transmits only what its pivot holds, so a keen head on a long
// haft delivers NOTHING through a lashing that lets it walk off. The tool is its weakest
// link -- and you feel it as the head shifting, working loose, finally sailing past your ear.
//
// And the circle closes here. A haft is worked wood, and the good wood -- timber -- is
// locked behind the very axe you are building (fuel.h, gather.cpp). So your first haft
// can only be a hand-cut SAPLING: springy, splitty, a joint that will never fully seat.
// The first axe is crude by necessity -- and it is still just enough to bring down one
// trunk. But a felled trunk is a green log, not a haft: the axe wins the wood, and the
// ADZE -- the same edge hafted the other way (dress.h) -- pares that log into the timber
// every tool after is cut from. So the bootstrap does not turn on one tool but on one
// EDGE hafted twice, and this slice is both turns: swing a sapling axe to fell the tree,
// then set the same head as an adze and dress the log into the haft you could never reach.
//
// -- The gesture. ----------------------------------------------------------
//
// gather's honest gauge was the STAND, knap's was the STONE, char's was the SMOKE. This
// scene's gauge is the HEAD ON THE HAFT and the SWING itself: you do not read a
// "durability" number, you feel how the head sits -- it rocks, it is barely on, it does
// not move -- and then you swing at the trunk and learn what the joint was worth. Rush
// the binding and the head flies off with the edge undelivered; work it and the same
// edge fells the tree. Nobody tells you to seat it before you swing, or that a pick is
// the wrong tool for a tree: you swing, and the joint teaches you.
//
// Build: make haft   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "haft.h"
#include "dress.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// The head you carried from the knapping, the sticks from the gathering, and the pace
// of the work. Authored game pacing -- the counterpart to knap's strike rates -- no
// test or finding reads these. What they play against IS the finding: a sapling joint
// can never seat as a timber one can, and force fells, not sharpness.
static constexpr double HEAD_MASS    = 0.40;   // kg, the worked stone bit knap.cpp left you.
static constexpr double SAPLING_LEN  = 0.55;   // m, a green stick's reach -- short, all you have before the axe.
static constexpr double TIMBER_LEN   = 0.70;   // m, a seasoned haft split from a felled trunk -- longer, stiffer.

static constexpr double BIND_RATE    = 0.10;   // per second of working the joint; ~2s to catch, ~6.5s to seat.
static constexpr double BIND_LASHED_AT = 0.20; // below this the head is barely on -- it flies off on a swing.
static constexpr double BIND_SEATED_AT = 0.65; // at/above this the head is seated -- it does not walk out.
static constexpr double LASH_LOOSEN  = 0.14;   // a lashed (unseated) joint works loose this much per loaded swing.

static constexpr double TREE_FULL    = 1.0;    // "depth" of the standing trunk; a swing's bite eats into it.
static constexpr double CHOP_SCALE   = 3.0;    // how far a unit of delivered bite drives the cut.
static constexpr double SPECK        = 1e-6;

// Map the continuous binding work to haft.h's discrete Bind gate.
static Bind bind_of(double work) {
    if (work < BIND_LASHED_AT) return BIND_NONE;
    if (work < BIND_SEATED_AT) return BIND_LASHED;
    return BIND_SEATED;
}

// ---------------------------------------------------------------------------
// Terminal. The same raw tty every slice shares (pan -> ... -> knap).

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

static const char* kind_name(ToolKind k) {
    return k == AXE ? "axe" : k == PICK ? "pick" : "adze";
}

// How the head sits -- the honest gauge, read as feel, not a number.
static const char* joint_face(double work) {
    switch (bind_of(work)) {
        case BIND_NONE:   return "the head is barely on -- it rocks loose in your hand";
        case BIND_LASHED: return "the head is lashed, but you can still feel it shift";
        case BIND_SEATED: return "the head is seated tight -- it does not move at all";
    }
    return "";
}

static const int PANEL_COL = 46;

static void draw_panel(double bind_work, ToolKind kind, bool have_timber,
                       double tree_left, bool felled, const char* last_swing) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };
    auto bar16 = [&](double frac, char full_ch) {
        static char b[17];
        const int pf = (int)std::lround((frac < 0 ? 0 : frac > 1 ? 1 : frac) * 16.0);
        for (int i = 0; i < 16; ++i) b[i] = i < pf ? full_ch : ' ';
        b[16] = '\0';
        return b;
    };

    line("the tool in your hands");
    line("%s", "------------------------------");
    line("  head    a knapped edge, ~%.0f deg -- as keen as stone gets", STONE_EDGE_FLOOR);
    line("  haft    %s", have_timber ? "seasoned timber -- long and stiff" : "a green sapling -- short, springy");
    line("  it is a %s  (edge %s)", kind_name(kind),
         kind == AXE ? "across -- for felling" : kind == PICK ? "at a point -- for rock" : "in line -- for dressing wood");
    line("%s", "");

    // The joint: a bar you raise by working it, and the feel that reads off it.
    line("  the joint |%s|", bar16(bind_work, '='));
    line("  %s", joint_face(bind_work));
    line("%s", "");

    // The trunk -- the wall gather could not cross, here to be swung at.
    if (have_timber)
        line("  the tree  |%s| DOWN and dressed -- the timber is yours", bar16(0.0, '#'));
    else if (felled)
        line("  the tree  |%s| DOWN -- a raw log; dress it with the adze [3]+[t]", bar16(0.0, '#'));
    else
        line("  the tree  |%s| standing -- swing to fell it", bar16(tree_left / TREE_FULL, '#'));
    line("%s", "");

    line("%s", last_swing ? last_swing : "you have not swung yet");
}

// ---------------------------------------------------------------------------

static const char* AMBIENT[] = {
    "the stone head weighs almost nothing in the fist and everything on the haft",
    "the green sapling flexes when you heft it -- it will never be as true as split timber",
    "the trunk stands there, straight and full of the wood a haft is cut from, out of reach",
    "you wrap the sinew once more and test the head with your thumb -- it still gives, a little",
    "a good edge and a bad binding is a rock tied to a stick, and you have made exactly that",
    "somewhere the same trunk you cannot fell has dropped another dead branch for your hands",
};
static const int N_AMBIENT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

static void draw(double bind_work, ToolKind kind, bool have_timber, double tree_left,
                 bool felled, bool binding, double t, const char* ambient,
                 const char* nag, const char* last_swing) {
    std::printf("\033[H\033[J");
    std::printf("\n   %-52s  %2d:%02d\n\n", ambient ? ambient : "", (int)t / 60, (int)t % 60);
    std::printf("   the edge of the clearing -- a stone head, a stick, and one standing tree\n\n");
    std::printf("   %s\n\n", nag ? nag : "");
    std::printf("   [l] work the joint%s   set head:  [1] axe  [2] pick  [3] adze\n",
                binding ? "  (binding...)" : "");
    std::printf("   [w] swing at the tree   [t] dress a haft from the log   [r] keep it and go   [q] walk away\n");
    draw_panel(bind_work, kind, have_timber, tree_left, felled, last_swing);
    std::printf("\033[24;1H");
    std::fflush(stdout);
}

// The reckoning: what you built, whether the lever delivered, and the two walls the
// scene teaches -- leaning on haft.h's gates, not on numbers restated here.
static void report_haft(bool felled, bool have_timber, bool ever_flew, ToolKind kind, double best_bite) {
    std::printf("   You lower the tool and take stock of what you made.\n\n");

    if (felled && !have_timber) {
        std::printf("   The trunk came down -- a stone edge that only scraped in your fist, swung on the\n"
                    "   end of a stick, felled a tree, and the edge never got keener to do it. That is\n"
                    "   the haft's gift: not a sharper edge but the FORCE a lever puts behind the one\n"
                    "   you had. Gather's wall -- hands never win timber -- is the wall this axe walked\n"
                    "   through.\n\n");
        std::printf("   But a felled trunk is a green log, not a haft, and the axe that felled it cannot\n"
                    "   pare it -- its edge crosses the grain a haft must follow. You stopped one step\n"
                    "   short: the SAME edge, hafted the other way as an adze, is what dresses the log\n"
                    "   into the stock every tool after rides. Knap once, haft twice.\n\n");
        return;
    }

    if (felled) {   // felled AND dressed -- both turns of the bootstrap
        std::printf("   The trunk came down, and then you dressed it. First the axe: a floored edge on a\n"
                    "   sapling, swung across the grain with the FORCE a lever lends, felling the tree\n"
                    "   gather's bare hands could not touch. Then the adze: the same edge hafted in line,\n"
                    "   paring the fallen log true ALONG its grain -- no force needed, only the edge.\n\n");
        std::printf("   Two verbs, one stone. The bootstrap has turned: hands -> stone edge -> sapling\n"
                    "   axe -> the felled trunk -> the adze -> a seasoned timber haft, the stock every\n"
                    "   tool after is cut from -- and the edge never got keener at any step.\n\n");
        return;
    }

    if (kind != AXE) {
        std::printf("   You never felled it -- you were swinging a %s. A point wins hard rock and an\n"
                    "   adze dresses wood already down; neither bites across a standing trunk. Same\n"
                    "   head, same haft, same swing -- the geometry of the head is the tool.\n\n",
                    kind_name(kind));
        return;
    }

    if (ever_flew) {
        std::printf("   The head flew off. You had an edge keen enough and a haft long enough, and\n"
                    "   you delivered none of it, because the joint would not hold -- and the tool is\n"
                    "   only ever as good as its weakest link. A rock tied badly to a stick is not a\n"
                    "   worse axe; it is no axe. Work the joint before you swing.\n\n");
        return;
    }

    std::printf("   The tree still stands. Your best swing landed about %.2f of what felling wants,\n"
                "   and it wanted more -- a better-seated joint, or the timber haft you do not have\n"
                "   yet. You felt the ceiling; it was the joint, not the edge.\n\n", best_bite / FELL_ENERGY);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "haft: needs a terminal. Run it yourself: cd core && make haft\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   knap left you an edge and a wall: a stone edge is only ever so keen. You will\n"
        "   not get under that wall here. You will get AROUND it -- by putting the edge on\n"
        "   a handle, where the same edge does what your fist never could.\n\n"
        "   You have the knapped head and a green sapling for a haft. (The good wood --\n"
        "   timber -- is still standing, because felling it needs the very axe you are\n"
        "   about to make. The sapling is what you have, so the sapling is what it rides.)\n\n"
        "   Bind the head to the haft: work the joint until it seats. Rush it and the head\n"
        "   is barely on -- swing and it flies off, the edge undelivered. Seat it, choose\n"
        "   an axe, and swing at the trunk. A lever's blow grows with the square of its\n"
        "   length, so the tree yields to the FORCE the haft puts behind your floored edge,\n"
        "   not to any sharpness. Bring it down -- then set the same edge as an ADZE and\n"
        "   dress the fallen log along its grain into the haft you could never reach.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    double bind_work = 0.0;
    ToolKind kind = AXE;
    Helve helve = EDGE_ACROSS;
    bool have_timber = false;
    double tree_left = TREE_FULL;
    bool felled = false, binding = false, ever_flew = false;
    double best_bite = 0.0;
    double t = 0.0;

    const StoneEdge head { HEAD_MASS, STONE_EDGE_FLOOR, true };  // knap's floored edge, carried in.

    const char* ambient = AMBIENT[0];
    double ambient_t = 0;
    std::string nag = "Work the joint first: press [l] until the head seats. A rushed joint throws the head.";
    double nag_t = 10;
    std::string last_swing;

    // Swing the current tool at the trunk. Everything the swing decides is read through
    // haft.h -- the assembly, the joint gate, the delivered bite, the fell wall.
    auto swing = [&]() {
        binding = false;
        const HaftStock stock = have_timber ? TIMBER : SAPLING;
        const double len = have_timber ? TIMBER_LEN : SAPLING_LEN;
        const Bind bind = bind_of(bind_work);
        const Hafted tool = haft(head, len, stock, bind, helve);

        if (tool.kind != AXE) {   // wrong tool for a tree -- the geometry gate
            last_swing = tool.kind == PICK
                ? "the pick's point glances off the bark -- a pick is for rock, not a tree"
                : "the adze skips down the trunk -- an adze dresses felled wood, it does not fell";
            nag = "That head is not set to fell. Press [1] to set it as an axe -- edge across the swing.";
            nag_t = t + 6;
            return;
        }
        if (!tool.sound) {        // the joint gate -- the head walks off
            ever_flew = true;
            last_swing = "the head sails off the haft and into the brush -- all that edge, delivered to nothing";
            nag = "The joint would not hold. Work it more with [l] before you swing -- the tool is its weakest link.";
            nag_t = t + 7;
            bind_work *= 0.5;     // and you have to re-fetch and re-bind it
            return;
        }

        const double bite = tool.bite();
        if (bite > best_bite) best_bite = bite;
        if (tool.fells()) {
            tree_left -= bite * CHOP_SCALE;
            if (bind == BIND_LASHED) bind_work -= LASH_LOOSEN;   // an unseated joint works loose under load
            if (tree_left <= SPECK) {
                tree_left = 0.0; felled = true;
                last_swing = "the trunk cracks, leans, and comes down -- a green log at your feet, not yet a haft";
                nag = "It is DOWN -- but a felled trunk is a raw log. Set the head as an ADZE [3], seat it, then [t] to dress a haft.";
                nag_t = t + 10;
            } else {
                last_swing = bind == BIND_LASHED
                    ? "the edge bites deep -- but you feel the lashing give a little with the blow"
                    : "the edge bites deep and clean, a wedge of wood springing from the cut";
            }
        } else {
            last_swing = "the edge marks the trunk but barely -- not enough behind it to fell";
            nag = "Barely a dent. Seat the joint harder, or you want the longer timber haft you do not have yet.";
            nag_t = t + 6;
        }
    };

    bool running = true, quit = false, kept = false;
    while (running) {
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == 'q') { running = false; quit = true; }
            else if (c == 'r') { running = false; kept = true; }
            else if (c == 'l') { binding = true; }
            else if (c == '1') { kind = AXE;  helve = EDGE_ACROSS; binding = false; }
            else if (c == '2') { kind = PICK; helve = HEAD_POINT;  binding = false; }
            else if (c == '3') { kind = ADZE; helve = EDGE_INLINE; binding = false; }
            else if (c == 'w') { swing(); }
            else if (c == 't') {
                if (have_timber) {          // already dressed -- re-haft on the timber you won
                    bind_work = 0.0;
                    nag = "You split a fresh timber haft from the trunk and start the joint clean -- it seats far tighter than the sapling.";
                    nag_t = t + 7;
                } else if (!felled) {       // the locked beat, like gather's [f] -- nothing felled to dress
                    nag = "There is no trunk to dress -- it is still standing. That is the tree you are trying to fell with the axe.";
                    nag_t = t + 6;
                } else {                    // a log on the ground: the adze's verb, not the axe's (dress.h)
                    const Hafted cur = haft(head, SAPLING_LEN, SAPLING, bind_of(bind_work), helve);
                    if (can_dress(cur)) {   // a sound adze pares the log along the grain -> timber
                        have_timber = true;
                        bind_work = 0.0;    // a fresh timber haft is a fresh joint -- but the ceiling is higher now
                        last_swing = "the adze skims the log along its grain, paring it flat and true -- a haft, at last";
                        nag = "Dressed. The same edge that felled the trunk, hafted the other way, made the haft -- knap once, haft twice. Now re-seat it.";
                        nag_t = t + 10;
                    } else {
                        nag = "The trunk is down but it is a raw green log. The axe that felled it cannot pare it -- set the head as an ADZE [3], seat it, then [t].";
                        nag_t = t + 8;
                    }
                }
            }
        }

        const double dt = 0.05;
        t += dt;

        if (binding) {
            bind_work += BIND_RATE * dt;
            if (bind_work > 1.0) bind_work = 1.0;
        }

        if (t > ambient_t) { ambient = AMBIENT[(int)(t / 43) % N_AMBIENT]; ambient_t = t + 43; }
        if (t > nag_t) nag.clear();

        draw(bind_work, kind, have_timber, tree_left, felled, binding, t, ambient,
             nag.empty() ? nullptr : nag.c_str(), last_swing.empty() ? nullptr : last_swing.c_str());
        nap(dt);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || !kept) {
        std::printf("   You set the head and the stick down apart from each other and go.\n\n");
        return 0;
    }

    report_haft(felled, have_timber, ever_flew, kind, best_bite);
    std::printf("   It cost you %d minutes and %d seconds at the tree.\n\n",
                (int)t / 60, (int)t % 60);
    return 0;
}
