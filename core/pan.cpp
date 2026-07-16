// Phase A, step 1.5: play the pan.
//
// This is not a test. It is the smallest possible answer to the only question
// `core/` cannot answer by passing: is the grade/recovery tradeoff a *game*, or
// is it a spreadsheet? Nobody can settle that by arguing. Somebody has to kneel
// down and wash a pan and find out whether they want to do it again.
//
// It runs on exactly the same physics as test_separation.cpp. There is no
// difficulty curve, no balance pass, and no number in here that was chosen to
// make the experience pleasant. What this file adds to the model is *legibility*
// and nothing else. (DESIGN.md: a physical detail earns its place if it is true,
// if it creates a decision, and if it is legible without a wiki.)
//
// -- The second version, and why. -------------------------------------------
//
// The first version was a command parser. You typed `wash 3` and it called
// separate() once with a cut chosen from a table of seven. Ross played it and
// reported, exactly: "learning the interface was most of the work... I am more
// interested in what actions I need to take to win the game than to learn what
// this is teaching me... since there was no persistent display showing me data
// that changed as I worked the pan, I wasn't seeing the consequences of my
// actions in realtime."
//
// He had found a defect in the model, not the presentation. A wash is not one
// call to separate() at a cut the operator picks. **The cut is the instantaneous
// speed of the water**, it changes continuously while the hands move, and the
// only decision a panner actually makes is *when to stop* — which he makes over
// and over, with his eyes, while it is happening. The first version simulated the
// outcome and made the player type it. So of course he optimised for the outcome.
//
// Therefore: separate() is called every tick, at whatever cut the player's hand
// is producing right now, and the pan is redrawn. The table of seven wash
// strengths is deleted. It is replaced by one time constant. That is seven
// authored numbers traded for one, which is the same trade the settling-velocity
// rewrite made, and it is starting to look like a law.
//
// -- The third version, and why. --------------------------------------------
//
// The second version showed weight, colour, and what left the pan, and nothing
// else. The comment that used to sit here said: "He is never shown a grade.
// Grade is an assay, and there is no laboratory in a river. Put a live
// percentage on the screen and he will optimise the percentage and never look
// at the sand again."
//
// That was two claims wearing one coat, and only one of them was true.
//
// The true one: a *score* — one number, with an arrow pointing up — is a thing
// the player optimises instead of playing. That danger is real and it is why
// there is still no score in here, no target, and no win.
//
// The false one: that the composition of the pan is a score. It is not. It is
// the **state**, and the state is the whole claim this project makes. There is
// no `iron ore` item; there is a vector over mineral phases, and if we hide it
// then the one thing that distinguishes this from every other panning minigame
// ever shipped is invisible to the person we built it for. A simulation that
// conceals its state is a video with a keyboard attached.
//
// And "there is no laboratory in a river" was answering a question nobody asked.
// The panner has no assay. The panner is not alone. README claim #3 is a
// co-labouring AI that stands next to you and knows things, and *what is in this
// dirt* is precisely the kind of thing it is for. Asking it is not cheating. It
// is the relationship.
//
// So: the assay panel is live, it is always on, and it shows grade and recovery
// **together**, for every phase, because those are the two halves of the law and
// showing one without the other is what a score would be. Grade rises. Recovery
// only ever falls. Watching those two columns move in opposite directions, in
// real time, under your own hand, is the entire lesson of Era 0 and it took
// eleven lines of printf.
//
//   (Ross, having played v2: "i do not see the contents of the sand, and this is
//   probably the most important part. like asking you for the details of an
//   object... as i pan, the percentages of the minerals should change right in
//   front of me.")
//
// -- The fourth version: the tailings tub. ----------------------------------
//
// The wash threw everything over the lip into the void, and the void does not
// give it back. But a panner does not pour tailings into the river; he pours
// them into a tub at his knee, because a hard wash throws ore and the sand in
// that tub can be washed again. Only the mud escapes him — slimes stay in
// suspension and go downstream, and no tub catches them. That is the same mud
// line the pan itself cannot cross (levigation's job, not the pan's), so the
// verb authors no number: what leaves splits on the existing Wentworth boundary
// — sand and gravel settle into the tub as re-pannable tailings, the mud is
// gone.
//
// [r] tips the tub back into the pan. This is the second half of "when do I
// stop": over-wash and the tub glitters, and you may re-pan it — recover the
// ore, but re-mix the gangue you had just rejected, and spend the minutes — or
// leave it. The finest ore, ground to slime and washed downstream, never comes
// back; the grade/recovery law keeps its teeth. This is the re-panning verb the
// design carried open. The panner's skill was never the wrist. It is this.
//
// -- The fifth version: the pan left the terminal. --------------------------
//
// Every number, every deposit, every nag and ambient line above now lives in
// core/pan_game.h as a UI-free PanGame the UE client also drives (its feedback
// text lands in the on-screen chat panel). This file is what it always was --
// the terminal front-end -- but the model underneath it is shared, not copied,
// so the pan has exactly one physics wherever it is played.
//
// Build: make pan   (needs a terminal; it reads single keypresses)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <ctime>
#include <cstdarg>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "pan_game.h"

using namespace wrought;

// ---------------------------------------------------------------------------
// Terminal.

static struct termios saved_tty;
static bool tty_raw = false;

static void restore_tty() {
    if (tty_raw) { tcsetattr(STDIN_FILENO, TCSANOW, &saved_tty); tty_raw = false; }
    std::printf("\033[?25h");   // cursor back on
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
    std::printf("\033[?25l");   // cursor off
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
// The pan, drawn. The persistent display, made of dirt rather than numbers: the
// bottom of the pan fills with '#' as the black sand concentrates -- a live grade
// readout that never says a number. All state is read from the PanGame.

static const int PAN_W = 30;
static const int PANEL_COL = 42;

// Grade and recovery, side by side, one row per mineral. Neither column is a
// score, because they move in opposite directions and you cannot have both.
static void draw_assay(const PanGame& g) {
    int row = 2;
    auto line = [&](const char* fmt, ...) {
        std::printf("\033[%d;%dH", row++, PANEL_COL);
        va_list ap; va_start(ap, fmt); std::vprintf(fmt, ap); va_end(ap);
    };

    line("what the pan holds");
    line("%-10s %6s %7s %6s", "", "grams", "grade", "kept");
    line("%s", "----------------------------------");

    for (const PanGame::AssayRow& r : g.assay_rows()) {
        if (r.gone) { line("%-10s %6s %7s %6s %c", r.id, "-", "-", "gone", ' '); continue; }
        line("%-10s %6.1f %6.1f%% %5.0f%% %c", r.id, r.grams, r.grade, r.kept, r.arrow);
    }

    const Substance& pan = g.pan();
    const Substance& origin = g.origin();
    const double m = pan.total_mass();
    const double blk = pan_black_mass(pan), blk0 = pan_black_mass(origin);
    const double lok = pan_locked_black(pan);
    line("%s", "----------------------------------");
    line("%-10s %6.1f %6.1f%% %5.0f%%", "black sand", blk * 1000.0,
         m > 1e-9 ? 100.0 * blk / m : 0.0,
         blk0 > 1e-9 ? 100.0 * blk / blk0 : 0.0);
    if (lok * 1000.0 > 0.05)
        line("  %.1f g of it is locked in quartz", lok * 1000.0);
    line("  washed away: %.0f g", (origin.total_mass() - m) * 1000.0);
}

static void draw(const PanGame& g) {
    std::printf("\033[H\033[J");

    const Substance& pan = g.pan();
    const double m  = pan.total_mass();
    const double bf = g.black_frac();
    const double gr = m > 1e-9 ? pan_bin_mass(pan, GRAVEL) / m : 0.0;
    const double fn = m > 1e-9 ? pan_mud_mass(pan) / m : 0.0;
    const double t  = g.time_s();

    std::printf("\n   %-46s  %2d:%02d\n\n", g.ambient().c_str(), (int)t / 60, (int)t % 60);

    // the pan, in section. black sand lies under the pale, because it is heavier.
    const int fill  = (int)std::lround(PAN_W * std::fmin(1.0, m / 2.0));
    const int black = (int)std::lround(fill * bf);

    std::printf("   \\%s/\n", std::string(PAN_W + 2, '_').c_str());
    for (int r = 0; r < 3; ++r) {
        std::printf("    |");
        for (int i = 0; i < PAN_W; ++i) {
            const bool in = i >= (PAN_W - fill) / 2 && i < (PAN_W - fill) / 2 + fill;
            if (!in) { std::putchar(' '); continue; }
            if (r == 2)                     std::putchar(i < (PAN_W - fill) / 2 + black ? '#' : '.');
            else if (r == 1 && fill > 8)    std::putchar(i < (PAN_W - fill) / 2 + black / 2 ? '#' : '.');
            else                            std::putchar(gr > 0.15 && (i % 5) == 0 ? 'o' : (fn > 0.20 ? '~' : ' '));
        }
        std::printf("|\n");
    }
    std::printf("    \\%s/\n\n", std::string(PAN_W, '_').c_str());

    // what is leaving, drifting right, this second and the last two.
    const PanGame::Plume* plume = g.plume();
    const int plume_head = g.plume_head();
    for (int k = 0; k < 3; ++k) {
        const PanGame::Plume& p = plume[(plume_head - k - 1 + 3) % 3];
        std::printf("      ");
        if (p.mass < 1e-7) { std::printf("\n"); continue; }
        const int n = (int)std::lround(std::fmin(20.0, p.mass * 4000.0));
        const char ch = p.fines > 0.5 ? '~' : '.';
        for (int i = 0; i < k * 4; ++i) std::putchar(' ');
        for (int i = 0; i < n; ++i) std::putchar(p.black > 0.25 ? '*' : (p.black > 0.08 ? '+' : ch));
        std::printf("\n");
    }

    std::printf("\n   the water: ");
    const int bars = (int)std::lround(g.cut_fraction() * 24.0);
    for (int i = 0; i < 24; ++i) std::putchar(i < bars ? '=' : ' ');
    if (g.cut() < 0.002) std::printf("  still");
    std::printf("\n");
    std::printf("   the pan:   %s\n", g.colour());
    std::printf("   the weight:%s%.0f g\n", " ", g.pan_mass_g());
    if (gr > 0.10) std::printf("   there are stones in it.\n");
    if (g.tub_mass_g() > 0.5)
        std::printf("   the tub:    %.0f g of tailings%s\n", g.tub_mass_g(),
                    g.tub_glitters() ? ", and it glitters" : "");

    std::printf("\n   %s\n", g.nag().c_str());
    std::printf("\n   [space] swirl  [p] pick a stone  [r] re-pan the tub  [k] keep it  [n] new pan  [q] quit\n");

    draw_assay(g);
    std::printf("\033[23;1H");
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------

int main() {
    if (!isatty(STDIN_FILENO)) {
        std::fprintf(stderr, "pan: needs a terminal. Run it yourself: cd core && make pan\n");
        return 1;
    }

    std::printf("\033[H\033[J\n"
        "   You are kneeling in a river.\n\n"
        "   You have a fired clay pan. The ground here is sand, and somewhere in it\n"
        "   is the black, heavy stuff that a fire will turn into iron. Most of what\n"
        "   you scoop up is quartz. It always will be.\n\n"
        "   Swirling is not a keypress. It is a motion you keep up. Tap [space]\n"
        "   over and over to hold the water moving; the moment you stop, it dies,\n"
        "   and a still pan separates nothing. How fast you tap is how hard the\n"
        "   water runs. What that is worth, nobody is going to tell you.\n\n"
        "   [press any key]\n");
    std::fflush(stdout);
    raw_tty();
    while (poll_key() < 0) nap(0.02);

    PanGame game;
    bool quit = false;

    while (game.running()) {
        // ---- input ---------------------------------------------------------
        for (int c; (c = poll_key()) >= 0; ) {
            if (c == ' ')       game.swirl();
            else if (c == 'q')  { quit = true; break; }
            else if (c == 'r')  game.repan_tub();
            else if (c == 'n')  game.new_deposit();
            else if (c == 'p')  game.pick_stone();
            else if (c == 'k')  game.keep();
        }
        if (quit) break;

        // ---- the world -----------------------------------------------------
        game.tick();
        game.drain_feedback();   // the terminal reads nag()/ambient() live; drop the log

        if (!game.running()) break;   // keep() ended the session
        draw(game);
        nap(PAN_DT);
    }

    restore_tty();
    std::printf("\033[H\033[J\n");

    if (quit || game.kept().total_mass() < 1e-6) {
        std::printf("   You stand up. Your knees hurt. You are carrying nothing.\n\n");
        return 0;
    }

    // The poke report -- computed by the shared core, printed here.
    for (const std::string& l : game.endgame_lines())
        std::printf("   %s\n", l.c_str());
    std::printf("\n");
    return 0;
}
