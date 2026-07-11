#pragma once

#include <cmath>
#include "substance.h"

// The valley floor as a FIELD, not a grid of nodes.
//
// This is the first SPATIAL model in the project. Until now every furnace charge
// was a closed box with no location; nothing in `wrought` had a position. Here the
// ground gets one. The design has always insisted (DESIGN.md "Place has meaning,
// for free", "World") that ore is a *deposit composition placed by hand*, never an
// "ore node" you plug a machine into -- and this file is what makes that concrete:
//
//   composition = f(x, y, tier).
//
// You point at the ground and take a scoop; what comes up is whatever the field
// says is there. Move, and the makeup changes -- because if the ground looks
// different, its makeup is different. Most of the valley is barren country rock;
// prospecting is walking it, sampling, and watching for the spot where the numbers
// on the panel twitch. An ore body is a region where a mineral's fraction rises
// smoothly toward a center and fades to nothing at the rim: graded, not a point.
//
// TWO THINGS THIS FILE EXISTS TO SAY, both of which the rest of the chain was
// already built to pay off:
//
//   1. DEPTH CHANGES THE MINERAL, NOT JUST THE AMOUNT. A copper body wears a
//      weathered oxide cap (cuprite) that a reducing fire smelts straight; dig
//      through it and the primary ore below is a SULFIDE (chalcocite) that the
//      furnace flatly refuses until it is roasted (smelt.h). "The easy gossans
//      were worked for an age before the deep sulfides" stops being prose and
//      becomes a fact about how far down you dug. A full-depth dig MIXES the
//      column -- oxide, barren middle, and sulfide in one spoil pile -- which is
//      exactly why the pile must be COBBED (sorted by eye) before the furnace.
//
//   2. CO-LOCATION IS THE GATE. Copper ground and tin ground are placed far
//      apart, so no single spot ever hands you both. Bronze therefore forces you
//      to travel or trade -- the historically decisive fact about tin (it is rare
//      and localized) made mechanical instead of asserted. Tin also breaks
//      copper's symmetry honestly: cassiterite is ALREADY an oxide, won from veins
//      and placer gravel, so it has no oxide-over-sulfide column -- its makeup is
//      the same at every depth.
//
// A NOTE ON THE ENGINE, because it drove a real design choice. Deforming terrain
// at runtime (voxels, carved pits) is the expensive, hardware-heavy, and -- for a
// zero-budget project -- not-free path. It is also unnecessary: the hole is pure
// cosmetics. The mechanic needs only a coordinate under the crosshair, this field,
// and a panel. So the ground is a cheap static plane (as pretty as you like --
// hills, grass, trees are static mesh that costs nothing at runtime), and DEPTH is
// not a carved z-axis but a discrete TIER the dig bar crosses stroke by stroke. A
// volumetric dig would be a purely visual upgrade that changes zero lines here,
// because the simulation only ever sees the Substance that comes up, never the
// hole it came from.
//
// AUTHORED, tracked in issue #28: the whole valley layout (deposit centers, radii,
// peak grades), N_TIER, the per-tier mineral assignment, and the linear falloff
// are game-world placeholders -- a human chose them, no correct value exists. The
// tests assert only structure (barren-by-default, monotone grading, depth-changes-
// mineral, copper/tin never co-located), so correcting any number changes no test.

namespace wrought {

// A point on the valley floor: meters from an arbitrary origin. There is no z --
// depth is a discrete tier, not a coordinate (see the header comment).
struct Place {
    double x = 0.0;
    double y = 0.0;
};

// Depth tiers a dig-stroke crosses, shallow to deep. SURFACE is the weathered cap
// (an oxide gossan over a copper body); DEEP is the unweathered rock (the primary
// sulfide the gossan sits on); MIDDLE is the leached zone between. Three is the
// fewest that can carry "oxide over barren over sulfide". AUTHORED (#28).
enum DepthTier { SURFACE, MIDDLE, DEEP, N_TIER };

// A hand-placed ore body. Its ore mineral can differ by tier (that is the whole
// oxide-over-sulfide point); BARREN means this body contributes no ore at that
// depth, only the country rock everything sits in. Grade is `peak_grade` at the
// center and fades linearly to zero (pure gangue) at `radius`. AUTHORED (#28).
inline constexpr int BARREN = -1;

struct Deposit {
    const char* id;
    double cx, cy;             // center on the valley floor, m
    double radius;             // m; ore grade fades to background at the rim
    double peak_grade;         // ore mass fraction at the dead center
    int tier_mineral[N_TIER];  // the ore phase at each depth, or BARREN
};

// The valley. Two bodies, placed far apart so the co-location gate is real: a
// copper hill (oxide cap, leached middle, sulfide root) and, a few hundred meters
// off, a tin creek (cassiterite, the same at every depth -- no sulfide column).
// This is the concrete form of DESIGN.md's "six hand-authored deposit
// compositions"; more can be added the same way, no procedural generation.
inline constexpr Deposit DEPOSITS[] = {
    {"copper-hill", 0.0,   0.0,  40.0, 0.55, {CUPRITE, BARREN, CHALCOCITE}},
    {"tin-creek",   300.0, 120.0, 25.0, 0.45, {CASSITERITE, CASSITERITE, CASSITERITE}},
};

// A scoop of ground: the makeup at (`at`, `tier`), rendered as `mass` kg of coarse
// grains. `mass` is the tool's bite -- bare hands take a little to read the panel,
// a shovel hauls perhaps ten times as much of the very same makeup.
//
// The grains land FREE in the GRAVEL bin: coarse, because cobbing (the eye-and-
// fingers sort that the pile is destined for) works only on coarse liberated
// material; free, because real dug ore is locked in composite rock and would need
// crushing to liberate first -- a simplification skipped here and tracked in #28.
inline Substance sample(Place at, int tier, double mass) {
    Substance s;
    double ore = 0.0;
    for (const Deposit& d : DEPOSITS) {
        const int mineral = d.tier_mineral[tier];
        if (mineral == BARREN) continue;
        const double dx = at.x - d.cx, dy = at.y - d.cy;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double falloff = dist < d.radius ? 1.0 - dist / d.radius : 0.0;
        const double richness = d.peak_grade * falloff;
        if (richness <= 0.0) continue;
        s.freegrain[mineral][GRAVEL] += mass * richness;
        ore += richness;
    }
    if (ore > 1.0) ore = 1.0; // deposits are placed apart, so this never bites --
                              // it only guarantees the gangue term stays non-negative.
    // Everything that is not ore is barren country rock -- quartz gangue. This is
    // why most of the valley reads as nothing on the panel, and why even a rich
    // scoop is mostly waste you must cob away before it is worth any fuel.
    s.freegrain[QUARTZ][GRAVEL] += mass * (1.0 - ore);
    return s;
}

// A full-depth dig at one spot: the shovel crosses every tier and the spoil pile
// MIXES them. This is the mixed column -- oxide from the cap AND sulfide from the
// root, plus the barren middle, all heaped together. It is the richer haul and the
// harder problem at once: it must be COBBED (sorted by eye) into oxide, sulfide,
// and waste before the furnace, because the oxide smelts straight and the sulfide
// must first be roasted. `mass` is the bite taken from each tier.
inline Substance dig_column(Place at, double mass) {
    Substance pile;
    for (int t = 0; t < N_TIER; ++t) pile.add(sample(at, t, mass));
    return pile;
}

} // namespace wrought
