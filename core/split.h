#pragma once

#include "haft.h"   // the staircase this continues; the Trunk the axe felled becomes billets here.

// Splitting: a felled round log into billets, and why it needs no edge at all.
//
// dress.h felled the tree and, in one adze pass, handed you "seasoned TIMBER" -- it
// RIVED the round log and called the split billet SEASONED in the same breath, as if
// the adze did both. It does neither. Between the trunk on the ground and the haft in
// the hand are two whole verbs the adze was standing in for, and this file is the first
// of them: riving the log into billets. (The second is the calendar -- season.h.)
//
//   THE WEDGE, NOT THE EDGE. THE GRAIN SPLITS THE LOG; YOU ONLY OPEN IT.
//
//   Felling took a knapped edge on a lever (haft.h) and dressing will take one again
//   (dress.h) -- but splitting takes NEITHER. A froe or a hardwood glut driven by a
//   club is a WEDGE: it does not cut across the fibres, it opens a crack and the wood's
//   own grain runs it the length of the log. So a billet riven ALONG the grain keeps
//   its fibres full-length and unbroken -- stiff and strong, the whole reason a haft is
//   riven and not sawn; a billet taken ACROSS the grain has short runout and snaps like
//   the stick it looks like. The gate is the GRAIN, not force and not sharpness: no
//   fell-wall of energy, no edge floor. This is why the log can be split before the adze
//   even exists -- the one shaping act you own with nothing but two wedges and a mallet.
//
//   And you do not drive one split -- you rive the round log DOWN, halving and halving
//   into billets as THIN as you choose. That choice is not idle: thickness is the gate
//   season.h turns on. A billet rives thin and seasons in a season; the whole log rives
//   into nothing and would take a lifetime to dry (season.h, tau ~ thickness^2). So you
//   split thin here not only to follow the grain but to buy the drying time the next
//   station charges -- the two verbs are one decision seen twice.
//
// Splitting is the project's oldest law again: it AUTHORS NOTHING added. Like knapping,
// like levigation, the billet is not made -- it is what the log parts INTO, mass
// conserved (log = billets + riving waste). The model carries the one worked billet by
// the geometry that gates it (its grain and its thickness), not by a yield fraction, the
// same way knap() carries only FLAKE_FRACTION for the SIGN and not a per-blow census.
//
// Scope (design decision, 2026-07-12): a Trunk and a Stave are their own small stocks,
// like knap's Stone and haft's HaftStock -- not phase_table.h grains. Trunk and Rive move
// here from dress.h, their true home: riving is splitting's verb, not dressing's. The SIGN
// is the finding -- along the grain wins a billet, across it wins waste -- not any retained
// fraction; and the one authored number, the green log's as-felled wetness, is a species-
// variable stand-in (GREEN_MC, AUTHORED), read only as "soaking, far above fibre saturation."

namespace wrought {

// ---------------------------------------------------------------------------
// The felled trunk -- the axe's product (dress.h's Trunk, moved to its verb's home).
// Green, round, its grain running the length of it: a raw log, not a haft.
struct Trunk {
    double mass = 0.0;   // kg of green log, as felled
};

// Which way the billet is riven out of the log. The grain runs lengthwise, so a billet
// split ALONG it keeps its fibres full-length -- stiff and strong; one taken ACROSS the
// grain is short-grained -- it looks like a stick and snaps like one (no haft at all).
enum Rive {
    ALONG_GRAIN,   // riven with the grain: full-length fibres, a true stiff billet
    ACROSS_GRAIN   // cut across the grain: short runout, a billet that breaks under load
};

// A green log's moisture the instant it is split, dry basis. AUTHORED (issue #40): fresh
// sapwood runs anywhere from ~40% to well over 100% by species and season -- there is no
// one correct value to look up, only "soaking, far above fibre saturation." Read as the
// START of seasoning (season.h), never as a measurement.
inline constexpr double GREEN_MC = 0.75;

// ---------------------------------------------------------------------------
// A billet riven from the log. It carries exactly what gates the two stations after it:
// which way it was riven (the grain -- whether it is a haft at all), how thick it is (the
// clock season.h runs on), and how wet it is (green until the calendar dries it).
struct Stave {
    double mass = 0.0;         // kg (pass-through; conserved from the log, not a yield)
    double thickness = 0.0;    // m -- the riven cross-section; sets seasoning time (season.h)
    Rive   rive = ACROSS_GRAIN;
    double moisture = GREEN_MC; // dry-basis MC; soaking as split, falls only with time (season.h)
    bool   sound = false;      // a true billet (riven along the grain), or short-grained waste
};

// Rive a green trunk into a billet of the thickness you choose. No tool gate -- a wedge
// and a mallet, no edge -- so the only gate is the GRAIN: split ALONG it for a true billet,
// ACROSS it for waste. The billet comes off soaking green (GREEN_MC); drying it is the next
// verb (season.h), and a thinner billet will dry far faster there.
inline Stave split(const Trunk& log, Rive rive, double thickness) {
    Stave b;
    if (log.mass <= 0.0)  return b;   // no log to rive -- sound stays false
    if (thickness <= 0.0) return b;   // no billet without a cross-section
    b.mass = log.mass;                // conserved (one worked billet's-worth), not a yield fraction
    b.thickness = thickness;
    b.rive = rive;
    b.moisture = GREEN_MC;            // green off the wedge
    b.sound = (rive == ALONG_GRAIN);  // along the grain wins a billet; across wins waste
    return b;
}

} // namespace wrought
