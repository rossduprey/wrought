#pragma once

#include <cmath>
#include "substance.h"
#include "settling.h"   // SIZE_BOUNDS: the blow's PSD is cast against the size bins

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
// You point at the ground and take a bite; what comes up is whatever the field
// says is there. Move, and the makeup changes -- because if the ground looks
// different, its makeup is different. Most of the valley is barren country rock;
// prospecting is walking it, sampling, and watching for the spot where the numbers
// on the panel twitch. An ore body is a region where a mineral's fraction rises
// smoothly toward a center and fades to nothing at the rim: graded, not a point.
//
// THREE THINGS THIS FILE EXISTS TO SAY, all of which the rest of the chain was
// already built to pay off:
//
//   1. DEPTH CHANGES THE MINERAL, NOT JUST THE AMOUNT. A copper body wears a
//      weathered oxide cap (cuprite) that a reducing fire smelts straight; dig
//      through it and the primary ore below is a SULFIDE (chalcocite) that the
//      furnace flatly refuses until it is roasted (smelt.h). "The easy gossans
//      were worked for an age before the deep sulfides" stops being prose and
//      becomes a fact about how far down you dug. A full-depth dig MIXES the
//      column -- oxide, barren middle, and sulfide in one spoil pile.
//
//   2. LIBERATION IS A SIZE, NOT A TOLL. A pick blow does not yield one coarse
//      lump grade -- it shatters rock across a size DISTRIBUTION, and whether a
//      grain comes up free or still locked in gangue depends on how big it is
//      against how big the mineral's grains are. Placer tin is coarse and already
//      weathered free, so it comes up in the pan-ready fraction with the crusher
//      never touched; disseminated sulfide is ground to microns inside its rock,
//      so almost none of it frees at the face and the breaker is the only way in;
//      the weathered oxide cap sits between. This is why placers were panned by a
//      single hand while hard-rock ore built stamp mills -- and it is the substance
//      model finally obeying its own founding line, that "magnetite is free in the
//      sand and locked in the gravel" (substance.h). The breaker is not a formality
//      every gram must pass; it is what wins the fraction the blow left locked.
//
//   3. CO-LOCATION IS THE GATE. Copper ground and tin ground are placed far
//      apart, so no single spot ever hands you both. Bronze therefore forces you
//      to travel or trade -- the historically decisive fact about tin (it is rare
//      and localized) made mechanical instead of asserted. Tin also breaks
//      copper's symmetry honestly: cassiterite is ALREADY an oxide, won from veins
//      and placer gravel, so it has no oxide-over-sulfide column -- its makeup is
//      the same at every depth, and its grains are coarse and free.
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
// A CORRECTION, 2026-07-11. This file used to say "THE ORE COMES UP LOCKED --
// nothing is pre-liberated," dumping every gram into composite[mineral][GRAVEL].
// That was wrong from ignorance, and monitored play caught it: a pick (not a
// shovel -- a shovel MOVES loosened rock, a pick WINS it from the face) breaks
// rock across a size distribution, and the fine end of a placer's spoil is
// already-free grains a pan can take with no crusher at all. The earlier #28
// "shortcut" that was rightly killed was liberation INDEPENDENT of size (all ore
// free regardless, which trivialized the crusher). The truth is liberation as a
// FUNCTION of size, keyed to each mineral's real grain size -- which is not a
// shortcut but the whole premise of substance.h. The crusher stays essential for
// hard rock and is correctly irrelevant to placer.
//
// AUTHORED, tracked in issue #28: the whole valley layout (deposit centers, radii,
// peak grades -- now capped at COMPOSITE_TARGET_FRACTION), N_TIER, the per-tier
// mineral assignment, and the linear falloff are game-world placeholders -- a human
// chose them, no correct value exists. What is NOT a free knob: the per-mineral
// liberation sizes and the blow's size distribution below are grounded in the
// mineralogy (sources cited inline), and a scratchpad probe confirmed the finding
// they produce -- placer tin free at the face, sulfide locked -- does not depend on
// the distribution's exponent across its plausible range. The tests assert only
// structure (barren-by-default, monotone grading, depth-changes-mineral, copper/tin
// never co-located, and that placer frees at the face while the sulfide waits for
// the breaker), so correcting any layout number changes no test.

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
    double peak_grade;         // mineral mass fraction at the dead center. Capped
                               // at COMPOSITE_TARGET_FRACTION (0.5): the ore in the
                               // locked fraction is half-mineral composite rock, so
                               // a bite cannot read richer than a solid ore grain.
    int tier_mineral[N_TIER];  // the ore phase at each depth, or BARREN
};

// The valley. Two bodies, placed far apart so the co-location gate is real: a
// copper hill (oxide cap, leached middle, sulfide root) and, a few hundred meters
// off, a tin creek (cassiterite, the same at every depth -- no sulfide column).
// This is the concrete form of DESIGN.md's "six hand-authored deposit
// compositions"; more can be added the same way, no procedural generation.
inline constexpr Deposit DEPOSITS[] = {
    {"copper-hill", 0.0,   0.0,  40.0, 0.45, {CUPRITE, BARREN, CHALCOCITE}},
    {"tin-creek",   300.0, 120.0, 25.0, 0.45, {CASSITERITE, CASSITERITE, CASSITERITE}},
};

// The particle-size distribution of run-of-mine rock won by a pick blow. Breakage
// is comminution, and comminution follows the Gates-Gaudin-Schuhmann law: the
// cumulative mass finer than size x is (x/xmax)^m, with distribution modulus
// m ~ 0.8 for broken/crushed brittle rock (Schuhmann 1940; the modulus for crushed
// rock sits at ~0.7-1.0). xmax is the top of the coarsest bin. A blow makes mostly
// coarse rock and a thinning tail of fines -- which is the whole point: the fines
// carry FREE grains, the coarse carries LOCKED ones. SOURCED, not a game knob; the
// finding it drives is insensitive to m across 0.7-1.0 (scratchpad probe).
inline constexpr double PSD_MODULUS = 0.8;

// Mass fraction of a blow's spoil that lands in size bin s, GGS cast against the
// Wentworth bins and normalised over the four of them.
inline double psd_bin(int s) {
    const double xmax = SIZE_BOUNDS[GRAVEL][1];
    auto cum = [&](int b) { return std::pow(SIZE_BOUNDS[b][1] / xmax, PSD_MODULUS); };
    const double lo = (s == 0) ? 0.0 : cum(s - 1);
    return cum(s) - lo;
}

// The coarsest size bin at which a mineral's grains come FREE of gangue when rock
// is broken -- its liberation size, set by how the mineral occurs in the ground.
// Grains at or finer than this are parted from gangue by the blow itself; coarser
// grains ride up as LOCKED composite the breaker must crush. These are grain sizes,
// not game knobs:
//   CASSITERITE -- placer tin, coarse and already weathered free: liberated even at
//     GRAVEL. (~80% of world tin is placer, won by gravity precisely for its coarse
//     liberation size; Falconer, "A review of cassiterite beneficiation," Miner.
//     Eng. 2003.)
//   CUPRITE -- weathered oxide gossan: frees in sand and finer (~67% liberated at
//     +180 um; JESTEC 13(10), 2018).
//   CHALCOCITE -- disseminated sulfide down to ~1 um: only the CLAY tail frees at
//     the face; the rest waits for a fine grind (~32% liberated at a 200 um grind;
//     Physicochem. Probl. Miner. Process.).
// The default for an unrecognised ore is the finest bin -- assume it needs the full
// grind until its mineralogy says otherwise.
inline int liberation_bin(int mineral) {
    switch (mineral) {
        case CASSITERITE: return GRAVEL;
        case CUPRITE:     return SAND;
        case CHALCOCITE:  return CLAY;
        default:          return CLAY;
    }
}

// A bite of ground: the makeup at (`at`, `tier`), rendered as `mass` kg of broken
// rock. `mass` is the tool's bite -- bare hands take a little to read the panel, a
// pick wins perhaps ten times as much of the very same makeup. (The shovel is a
// LOADING tool, downstream in the carry loop; it moves the pile a pick has already
// won, it does not win rock from the face.)
//
// What comes up is a SIZE DISTRIBUTION, not one coarse grade, and liberation rides
// on it. For each ore body, the mineral travels in composite rock (half mineral,
// half gangue); the blow spreads that rock over the PSD, and every bin at or below
// the mineral's liberation size comes up already FREE -- grain and its gangue
// parted by the blow -- while coarser bins ride up LOCKED, still composite, still
// needing the breaker. So a placer's bite is pan-ready at the face; a disseminated
// sulfide's bite is rock you must carry and crush. The panel still reads the true
// GRADE (mass is conserved and the free/locked split does not touch it); what
// changed is that the grade is no longer all trapped behind the crusher.
inline Substance sample(Place at, int tier, double mass) {
    constexpr double f = COMPOSITE_TARGET_FRACTION;
    Substance s;
    double ore = 0.0; // mineral mass fraction of the bite, summed over bodies
    for (const Deposit& d : DEPOSITS) {
        const int mineral = d.tier_mineral[tier];
        if (mineral == BARREN) continue;
        const double dx = at.x - d.cx, dy = at.y - d.cy;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double falloff = dist < d.radius ? 1.0 - dist / d.radius : 0.0;
        const double richness = d.peak_grade * falloff;
        if (richness <= 0.0) continue;
        // It takes richness/f of composite rock to carry `richness` of mineral; the
        // blow spreads that rock over the PSD and liberation splits it by size.
        const double rock = mass * richness / f;
        const int lib = liberation_bin(mineral);
        for (int b = 0; b < N_SIZE; ++b) {
            const double here = rock * psd_bin(b);
            if (b <= lib) {
                s.freegrain[mineral][b] += f * here;           // mineral, freed by the blow
                s.freegrain[GANGUE][b]  += (1.0 - f) * here;   // its gangue, parted too
            } else {
                s.composite[mineral][b] += here;               // still locked rock
            }
        }
        ore += richness;
    }
    if (ore > f) ore = f; // deposits are placed apart, so this never bites -- it
                          // only keeps the free-gangue term below non-negative.
    // Everything not bound up in ore-bearing rock is barren country rock -- free
    // quartz gravel, spread over the same blow PSD. This is why most of the valley
    // reads as nothing on the panel, and why even a rich bite is mostly waste.
    const double gangue = mass * (1.0 - ore / f);
    for (int b = 0; b < N_SIZE; ++b) s.freegrain[QUARTZ][b] += gangue * psd_bin(b);
    return s;
}

// A full-depth dig at one spot: the pick crosses every tier and the spoil pile
// MIXES them. This is the mixed column -- oxide from the cap AND sulfide from the
// root, plus the barren middle, all heaped together. It is the richer haul and the
// harder problem at once: the free fines can be panned on the spot, but the locked
// coarse must be CRUSHED to free its grains and PANNED to part ore from waste
// (separate.h crush, then separate) before the furnace -- and the concentrate still
// carries both minerals, because the oxide smelts straight and the sulfide must
// first be roasted. `mass` is the bite taken from each tier.
inline Substance dig_column(Place at, double mass) {
    Substance pile;
    for (int t = 0; t < N_TIER; ++t) pile.add(sample(at, t, mass));
    return pile;
}

} // namespace wrought
