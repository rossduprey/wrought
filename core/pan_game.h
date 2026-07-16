#pragma once

// The pan, without a terminal.
//
// pan.cpp was one main(): the simulation, the ASCII drawing, raw-key input, and the
// feedback text, all fused. That fusion is why the game could only ever be a terminal.
// This header is the simulation and its *legibility* -- the wash physics, the three
// deposits, the assay ordering, the plume, and every line of feedback text the game
// says to you -- with printf, termios, and nanosleep removed. A host ticks it and reads
// it: the terminal front-end (pan.cpp, unchanged in behaviour) draws it in ASCII; the
// UE client (UWroughtPanGame) drives it from a pawn and shows its feedback in the chat
// panel. Same rule as the WroughtSimSubsystem seam: the physics has exactly one home.
//
// Nothing here authors a number the terminal game did not already author. The constants,
// the deposits, the nag thresholds, and the ambient lines are lifted from pan.cpp as-is;
// the only new thing is that feedback is *emitted* (queued for a caller to drain) instead
// of *drawn*.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <initializer_list>

#include "separate.h"

namespace wrought {

// --- The hand. (pan.cpp constants, verbatim.) ------------------------------
inline constexpr double PAN_DT           = 0.05;   // s per tick
inline constexpr double PAN_STROKE       = 0.032;  // m/s of water per wrist stroke. AUTHORED.
inline constexpr double PAN_DRAG_TAU     = 0.90;   // s. the water settles this fast. AUTHORED.
inline constexpr double PAN_PASS_TAU     = 1.50;   // s for one separation "pass". AUTHORED.
inline constexpr double PAN_MAX_CUT      = 0.170;  // you cannot swirl harder than this
inline constexpr double PAN_PICK_SECONDS = 6.0;

// --- The wash. A particle survives dt of washing with probability P^(dt/PASS_TAU). ---
inline Substance pan_wash_tick(Substance& pan, double cut, Substance& out_lost) {
    SeparatorParams sp = PAN;
    sp.cut_velocity = (cut < 1e-9) ? 1e-9 : cut;
    const double e = PAN_DT / PAN_PASS_TAU;
    const Substance top = exposed(pan, cut, skin_mass());

    Substance gone;
    const double T = pan.temperature;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s) {
            const double rf = std::pow(partition(free_velocity(p, s, T), sp), e);
            const double rc = std::pow(partition(composite_velocity(p, s, T), sp), e);
            gone.freegrain[p][s] = top.freegrain[p][s] * (1.0 - rf);
            gone.composite[p][s] = top.composite[p][s] * (1.0 - rc);
            pan.freegrain[p][s] -= gone.freegrain[p][s];
            pan.composite[p][s] -= gone.composite[p][s];
        }
    out_lost = gone;
    return gone;
}

// --- What a person crouching in a river can see. ---------------------------
inline double pan_black_mass(const Substance& s) {
    return s.phase_mass(MAGNETITE) + s.phase_mass(HEMATITE) + s.phase_mass(ILMENITE);
}
inline double pan_bin_mass(const Substance& s, int bin) {
    double m = 0.0;
    for (int p = 0; p < N_PHASE; ++p) m += s.freegrain[p][bin] + s.composite[p][bin];
    return m;
}
inline double pan_mud_mass(const Substance& s) { return pan_bin_mass(s, CLAY) + pan_bin_mass(s, SILT); }
inline double pan_black_frac(const Substance& s) {
    const double m = s.total_mass();
    return m > 1e-9 ? pan_black_mass(s) / m : 0.0;
}
inline double pan_locked_black(const Substance& s) {
    constexpr double f = COMPOSITE_TARGET_FRACTION;
    double t = 0.0;
    for (int p : {MAGNETITE, HEMATITE, ILMENITE})
        for (int b = 0; b < N_SIZE; ++b) t += f * s.composite[p][b];
    return t;
}
inline const char* pan_colour_word(double bf) {
    if (bf < 0.02) return "pale";
    if (bf < 0.08) return "grey";
    if (bf < 0.20) return "grey, with a dark tail";
    if (bf < 0.45) return "dark";
    if (bf < 0.75) return "black under the pale";
    return "black, and it glitters";
}

// --- The ground. ------------------------------------------------------------
struct PanScoopBuilder {
    Substance s;
    double demand[N_SIZE] {}, authored[N_SIZE] {};
    void put(int p, const double m[N_SIZE], const double lib[N_SIZE]) {
        constexpr double f = COMPOSITE_TARGET_FRACTION;
        if (p == GANGUE) { for (int k = 0; k < N_SIZE; ++k) authored[k] += m[k]; return; }
        for (int k = 0; k < N_SIZE; ++k) {
            s.freegrain[p][k] += m[k] * lib[k];
            s.composite[p][k] += m[k] * (1.0 - lib[k]) / f;
            demand[k] += (1.0 - f) * s.composite[p][k];
        }
    }
    Substance build() {
        for (int k = 0; k < N_SIZE; ++k) s.freegrain[GANGUE][k] = std::fmax(0.0, authored[k] - demand[k]);
        return s;
    }
};

inline constexpr double PAN_CLAY_PSD[N_SIZE] = {0.90, 0.10, 0.00, 0.00};

inline Substance pan_river_sand() {
    PanScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {2.0*frac*0.03, 2.0*frac*0.12, 2.0*frac*0.75, 2.0*frac*0.10};
        const double l[N_SIZE] = {lf, lf, ls, lg};
        b.put(p, m, l);
    };
    auto clay_row = [&](int p, double frac) {
        double m[N_SIZE]; const double l[N_SIZE] = {1.0, 1.0, 1.0, 1.0};
        for (int k = 0; k < N_SIZE; ++k) m[k] = 2.0 * frac * PAN_CLAY_PSD[k];
        b.put(p, m, l);
    };
    row(FELDSPAR,  0.100, 0.95, 0.90, 0.75);
    clay_row(KAOLINITE, 0.040);
    row(MAGNETITE, 0.080, 0.95, 0.88, 0.55);
    row(HEMATITE,  0.010, 0.90, 0.80, 0.50);
    row(GOETHITE,  0.020, 0.90, 0.80, 0.50);
    row(ILMENITE,  0.030, 0.92, 0.85, 0.55);
    row(PYRITE,    0.005, 0.85, 0.80, 0.50);
    row(CALCITE,   0.015, 0.90, 0.85, 0.60);
    row(QUARTZ,    0.700, 1.00, 1.00, 1.00);
    return b.build();
}

inline Substance pan_black_sand_bar() {
    PanScoopBuilder b;
    auto row = [&](int p, double frac, double lf, double ls, double lg) {
        const double m[N_SIZE] = {2.0*frac*0.01, 2.0*frac*0.09, 2.0*frac*0.88, 2.0*frac*0.02};
        const double l[N_SIZE] = {lf, lf, ls, lg};
        b.put(p, m, l);
    };
    row(MAGNETITE, 0.220, 0.97, 0.95, 0.70);
    row(ILMENITE,  0.060, 0.95, 0.92, 0.60);
    row(HEMATITE,  0.020, 0.92, 0.88, 0.50);
    row(FELDSPAR,  0.060, 0.95, 0.90, 0.75);
    row(QUARTZ,    0.640, 1.00, 1.00, 1.00);
    return b.build();
}

inline Substance pan_weathered_outcrop() {
    PanScoopBuilder b;
    const double q[N_SIZE] = {0.008, 0.032, 0.30, 1.06}, ql[N_SIZE] = {1, 1, 1, 1};
    const double f[N_SIZE] = {0.002, 0.008, 0.06, 0.20}, fl[N_SIZE] = {0.90, 0.90, 0.60, 0.30};
    const double m[N_SIZE] = {0.0008, 0.0032, 0.03, 0.26}, ml[N_SIZE] = {0.85, 0.85, 0.35, 0.10};
    const double h[N_SIZE] = {0.0004, 0.0016, 0.01, 0.024}, hl[N_SIZE] = {0.80, 0.80, 0.30, 0.10};
    b.put(FELDSPAR, f, fl); b.put(MAGNETITE, m, ml); b.put(HEMATITE, h, hl); b.put(QUARTZ, q, ql);
    return b.build();
}

// --- The assay: which phases the scoop holds, heaviest first, fixed at the dig. ---
struct PanAssay {
    int order[N_PHASE];
    int n = 0;
};
inline PanAssay pan_assay_of(const Substance& s) {
    PanAssay a;
    for (int p = 0; p < N_PHASE; ++p)
        if (s.phase_mass(p) > 1e-6) a.order[a.n++] = p;
    for (int i = 1; i < a.n; ++i) {
        const int k = a.order[i];
        int j = i - 1;
        while (j >= 0 && s.phase_mass(a.order[j]) < s.phase_mass(k)) { a.order[j+1] = a.order[j]; --j; }
        a.order[j+1] = k;
    }
    return a;
}

// The ambient river, said on a slow clock. (pan.cpp AMBIENT[].)
inline const std::vector<std::string>& pan_ambient_lines() {
    static const std::vector<std::string> lines = {
        "a bird you do not know the name of says the same thing four times",
        "the water is very cold above the wrist and not cold at all below it",
        "something moves in the willow behind you; you do not turn around",
        "rain starts, stops, and starts again without committing to it",
        "your knees have gone numb against the gravel",
        "downstream, the river makes a noise like a page turning",
        "the sun goes behind something and the sand stops glittering",
        "a stick goes past, faster than you expect",
        "the cold has reached the bone of your thumb",
        "somewhere behind you the fire is going out",
    };
    return lines;
}

// ---------------------------------------------------------------------------
// The pan game. Owns the state pan.cpp's main() held in locals; ticks the world;
// runs the same nag/ambient/plume/arrow logic; and *emits* feedback lines a host
// drains (drain_feedback) instead of drawing them.

class PanGame {
public:
    struct Plume { double mass = 0.0, black = 0.0, fines = 0.0; };

    // One row of the live assay panel, for a caller that renders it (grade/recovery
    // side by side, per phase, with the up/down arrow). `gone` == washed out.
    struct AssayRow {
        int phase = 0;
        const char* id = "";
        double grams = 0.0;
        double grade = 0.0;      // % of the pan
        double kept = 0.0;       // % of what the ground gave (recovery)
        char arrow = ' ';
        bool gone = false;
    };

    PanGame() { begin(0); }

    // Start (or restart) a fresh session on a deposit (0 river, 1 bar, 2 outcrop):
    // a clean clock, an empty tub and poke, and the ground re-scooped.
    void begin(int deposit) {
        t_ = 0.0;
        running_ = true;
        kept_ = Substance();
        nag_.clear(); nag_t_ = 0.0;
        ambient_t_ = 0.0; ambient_idx_ = 0;
        strokes_ = 0; taught_ = false;
        for (int k = 0; k < 3; ++k) plume_[k] = Plume{};
        plume_head_ = 0;
        plume_acc_m_ = plume_acc_b_ = plume_acc_f_ = plume_t_ = 0.0;
        feedback_.clear();
        load(((deposit % 3) + 3) % 3);
    }

    // --- Inputs (were single keypresses). -----------------------------------
    void swirl() { cut_ = std::fmin(PAN_MAX_CUT, cut_ + PAN_STROKE); ++strokes_; }

    void repan_tub() {
        if (tailings_.total_mass() < 1e-6) { set_nag("The tub is empty. Nothing to re-pan."); nag_t_ = t_ + 3; }
        else {
            pan_.add(tailings_);
            tailings_ = Substance();
            cut_ = 0.0;
            set_nag("You tip the tub back into the pan. The mud you lost stays lost.");
            nag_t_ = t_ + 4;
        }
    }

    void new_deposit() {
        load((deposit_ + 1) % 3);
        set_nag(deposit_ == 0 ? "You scoop up river sand."
              : deposit_ == 1 ? "You dig into the inside of the bend, where the sand is dark."
                              : "You break a lump off the outcrop and drop it in the pan.");
        nag_t_ = t_ + 4;
    }

    void pick_stone() {
        if (pan_bin_mass(pan_, GRAVEL) < 1e-5) { set_nag("There is nothing in there big enough to pick up."); nag_t_ = t_ + 3; }
        else {
            pan_ = screen(pan_, HAND_COB).undersize;
            t_ += PAN_PICK_SECONDS;
            set_nag("You pick out the stones you can see. There will be more.");
            nag_t_ = t_ + 4;
        }
    }

    // Tip the pan into the poke. Ends the session; returns whether anything was kept.
    bool keep() {
        if (pan_.total_mass() < 1e-6) { set_nag("There is nothing in the pan."); nag_t_ = t_ + 3; return false; }
        kept_.add(pan_);
        running_ = false;
        for (const std::string& l : endgame_lines()) feedback_.push_back(l);
        return true;
    }

    // --- The clock. Advances the world one FIXED tick (PAN_DT); queues any feedback
    // it produces. The sim is authored at a fixed step, so a real-time host must
    // accumulate elapsed seconds and call this floor(elapsed/PAN_DT) times, not scale
    // a step by a frame's dt. ---
    void tick() {
        if (!running_) return;

        Substance lost;
        pan_wash_tick(pan_, cut_, lost);
        // Over the lip: sand and gravel settle into the tub; mud stays in suspension, gone.
        for (int p = 0; p < N_PHASE; ++p)
            for (int s = 0; s < N_SIZE; ++s)
                if (s != CLAY && s != SILT) {
                    tailings_.freegrain[p][s] += lost.freegrain[p][s];
                    tailings_.composite[p][s] += lost.composite[p][s];
                }
        cut_ *= std::exp(-PAN_DT / PAN_DRAG_TAU);
        t_ += PAN_DT;

        // The plume, accumulated a fifth of a second at a time.
        plume_acc_m_ += lost.total_mass();
        plume_acc_b_ += pan_black_mass(lost);
        plume_acc_f_ += pan_mud_mass(lost);
        plume_t_ += PAN_DT;
        if (plume_t_ >= 0.20) {
            plume_[plume_head_] = {plume_acc_m_,
                                   plume_acc_m_ > 1e-9 ? plume_acc_b_ / plume_acc_m_ : 0.0,
                                   plume_acc_m_ > 1e-9 ? plume_acc_f_ / plume_acc_m_ : 0.0};
            plume_head_ = (plume_head_ + 1) % 3;
            if (plume_acc_b_ > 0.20 * plume_acc_m_ && plume_acc_m_ > 1e-5 && t_ > nag_t_) {
                set_nag("The cloud glitters. You are pouring it away."); nag_t_ = t_ + 2.5;
            } else if (plume_acc_f_ > 0.6 * plume_acc_m_ && plume_acc_m_ > 1e-5 && t_ > nag_t_) {
                set_nag("Whatever is in that mud is not coming back. It is too fine to catch."); nag_t_ = t_ + 4;
            }
            plume_acc_m_ = plume_acc_b_ = plume_acc_f_ = plume_t_ = 0.0;
        }

        if (strokes_ >= 12) taught_ = true;
        if (!taught_ && t_ > 8.0 && cut_ < 0.004 && pan_.total_mass() > 0.9 * origin_.total_mass() && t_ > nag_t_) {
            set_nag("The water has gone still, and a still pan separates nothing. Keep tapping.");
            nag_t_ = t_ + 4;
        }

        if (tailings_.total_mass() > 5e-4 && pan_black_frac(tailings_) > 0.15 && t_ > nag_t_) {
            set_nag("The tub at your knee has gone dark. You can re-pan it.");
            nag_t_ = t_ + 8;
        }

        // The grade arrows, on a slow clock (a flickering arrow is one nobody reads).
        if (t_ >= arrow_t_) {
            for (int p = 0; p < N_PHASE; ++p) {
                const double g = pan_.grade(p), d = g - grade_was_[p];
                arrow_[p] = d > 1e-4 ? '^' : d < -1e-4 ? 'v' : ' ';
                grade_was_[p] = g;
            }
            arrow_t_ = t_ + 0.5;
        }

        // The ambient river, one new line every 37 s -- emitted to the log on change.
        const int ai = (int)(t_ / 37) % (int)pan_ambient_lines().size();
        if (t_ > ambient_t_) {
            ambient_idx_ = ai;
            feedback_.push_back(pan_ambient_lines()[ambient_idx_]);
            ambient_t_ = t_ + 37;
        }

        if (t_ > nag_t_) nag_.clear();
    }

    // --- Feedback the caller drains (chat lines). Empties the queue. --------
    std::vector<std::string> drain_feedback() {
        std::vector<std::string> out;
        out.swap(feedback_);
        return out;
    }

    // --- Read-outs for a renderer / HUD. ------------------------------------
    bool     running()      const { return running_; }
    double   time_s()       const { return t_; }
    double   cut()          const { return cut_; }
    double   cut_fraction() const { return cut_ / PAN_MAX_CUT; }
    double   pan_mass_g()   const { return pan_.total_mass() * 1000.0; }
    double   black_frac()   const { return pan_black_frac(pan_); }
    const char* colour()    const { return pan_colour_word(pan_black_frac(pan_)); }
    bool     has_stones()   const { return pan_.total_mass() > 1e-9 && pan_bin_mass(pan_, GRAVEL) / pan_.total_mass() > 0.10; }
    double   tub_mass_g()   const { return tailings_.total_mass() * 1000.0; }
    bool     tub_glitters() const { return pan_black_frac(tailings_) > 0.08; }
    int      deposit()      const { return deposit_; }

    const Substance& pan()      const { return pan_; }
    const Substance& origin()   const { return origin_; }
    const Substance& tailings() const { return tailings_; }
    const Substance& kept()     const { return kept_; }
    const PanAssay&  assay()    const { return assay_; }
    const char*      arrows()   const { return arrow_; }
    const Plume*     plume()    const { return plume_; }
    int              plume_head() const { return plume_head_; }
    const std::string& ambient() const { return pan_ambient_lines()[ambient_idx_]; }
    const std::string& nag()    const { return nag_; }

    // The live assay, one row per phase in the fixed heaviest-first order.
    std::vector<AssayRow> assay_rows() const {
        std::vector<AssayRow> rows;
        const double m = pan_.total_mass();
        for (int i = 0; i < assay_.n; ++i) {
            const int p = assay_.order[i];
            const double mp = pan_.phase_mass(p);
            const double o  = origin_.phase_mass(p);
            AssayRow r;
            r.phase = p; r.id = PHASES[p].id; r.arrow = arrow_[p];
            if (mp * 1000.0 < 0.05 && o > 1e-6) { r.gone = true; rows.push_back(r); continue; }
            r.grams = mp * 1000.0;
            r.grade = m > 1e-9 ? 100.0 * mp / m : 0.0;
            r.kept  = o > 1e-9 ? 100.0 * mp / o : 0.0;
            rows.push_back(r);
        }
        return rows;
    }

    // The poke report -- the one place the game tells the truth, a scale and an eye.
    std::vector<std::string> endgame_lines() const {
        std::vector<std::string> out;
        const double m = kept_.total_mass();
        if (m < 1e-6) {
            out.push_back("You stand up. Your knees hurt. You are carrying nothing.");
            return out;
        }
        const double blk = pan_black_mass(kept_), py = kept_.phase_mass(PYRITE);
        char buf[256];
        out.push_back("You tip the pan into the poke.");
        std::snprintf(buf, sizeof(buf), "In the poke: %.0f g.", m * 1000.0); out.push_back(buf);
        std::snprintf(buf, sizeof(buf), "Of that, %.0f g is black sand -- %.0f%% of what you kept.",
                      blk * 1000.0, 100.0 * blk / m); out.push_back(buf);
        std::snprintf(buf, sizeof(buf), "You started with %.0f g of it in the ground. You have %.0f%%.",
                      pan_black_mass(origin_) * 1000.0, 100.0 * blk / std::fmax(pan_black_mass(origin_), 1e-9));
        out.push_back(buf);
        if (py / m > 0.004)
            out.push_back("There are brassy flecks in it that catch the light. They are pretty.");
        std::snprintf(buf, sizeof(buf), "The rest is pale sand -- %.0f g -- and the fire will not remove it.",
                      (m - blk - py) * 1000.0); out.push_back(buf);
        if (pan_bin_mass(kept_, GRAVEL) / m > 0.20)
            out.push_back("It is heavy for what it is. Most of it is stone you never picked out.");
        if (pan_black_mass(tailings_) * 1000.0 > 0.5) {
            std::snprintf(buf, sizeof(buf),
                "You leave a tub of tailings in the shallows. It still glitters; there was %.0f g "
                "of black sand in it you could have washed again.", pan_black_mass(tailings_) * 1000.0);
            out.push_back(buf);
        }
        out.push_back("Grade is what you kept. Recovery is what you did not lose. You cannot have both. Nobody can.");
        std::snprintf(buf, sizeof(buf), "It cost you %d minutes and %d seconds at the river.",
                      (int)t_ / 60, (int)t_ % 60); out.push_back(buf);
        return out;
    }

private:
    void load(int deposit) {
        deposit_ = deposit;
        pan_ = deposit == 0 ? pan_river_sand()
             : deposit == 1 ? pan_black_sand_bar()
                            : pan_weathered_outcrop();
        origin_ = pan_;
        tailings_ = Substance();
        cut_ = 0.0;
        assay_ = pan_assay_of(origin_);
        for (int p = 0; p < N_PHASE; ++p) { arrow_[p] = ' '; grade_was_[p] = origin_.grade(p); }
        arrow_t_ = t_;
    }

    void set_nag(const std::string& s) { nag_ = s; feedback_.push_back(s); }

    Substance pan_, origin_, kept_, tailings_;
    double cut_ = 0.0, t_ = 0.0;
    int deposit_ = 0;

    PanAssay assay_;
    char arrow_[N_PHASE] = {};
    double grade_was_[N_PHASE] = {};
    double arrow_t_ = 0.0;

    Plume plume_[3] = {};
    int plume_head_ = 0;
    double plume_acc_m_ = 0.0, plume_acc_b_ = 0.0, plume_acc_f_ = 0.0, plume_t_ = 0.0;

    int ambient_idx_ = 0;
    double ambient_t_ = 0.0;

    std::string nag_;
    double nag_t_ = 0.0;

    long strokes_ = 0;
    bool taught_ = false;

    std::vector<std::string> feedback_;
    bool running_ = true;
};

} // namespace wrought
