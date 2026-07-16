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
//      against how big the mineral's grains are. Disseminated sulfide is ground to
//      microns inside its rock, so almost none of it frees at the face and the
//      breaker is the only way in; the weathered oxide cap sits between; and a
//      PLACER (see #2b) hands you the mineral already free and already washed into
//      pannable sand, the breaker never touched. This is why placers were panned by
//      a single hand while hard-rock ore built stamp mills -- and it is the substance
//      model finally obeying its own founding line, that "magnetite is free in the
//      sand and locked in the gravel" (substance.h). The breaker is not a formality
//      every gram must pass; it is what wins the fraction the ground left locked.
//
//   2b. HOW THE ORE GOT THERE DECIDES ITS STATE. The same mineral is a different
//      problem depending on whether you break it out of rock or scoop it from a
//      creek. HARD ROCK comes up locked in composite, sized by the pick's blow --
//      the copper hill. A PLACER is rock a river already broke AND sorted: the dense
//      mineral, weathered free, dropped into a narrow heavy-SAND band while the
//      barren rock rode on as coarse cobble -- the tin creek. So the placer's tin is
//      free and pan-ready where it lies, and the wash that wins almost nothing at
//      the hard-rock hill wins most of it at the creek. This is the Origin field on
//      a Deposit, and it is the mechanical form of "won by panning gravel, not by
//      mining rock" (phase_table.h on cassiterite).
//
//   3. CO-LOCATION IS THE GATE. Copper ground and tin ground are placed far
//      apart, so no single spot ever hands you both. Bronze therefore forces you
//      to travel or trade -- the historically decisive fact about tin (it is rare
//      and localized) made mechanical instead of asserted. Tin also breaks
//      copper's symmetry honestly: cassiterite is ALREADY an oxide, won from veins
//      and placer gravel, so it has no oxide-over-sulfide column -- its makeup is
//      the same at every depth, and (as a placer) it comes up free in the sand.
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
// A SECOND CORRECTION, same day. The first pass modelled the tin creek as fresh
// hard rock too -- one PSD (GGS) for every deposit -- and set cassiterite's
// liberation to GRAVEL to make it "come up free." But a probe of the actual wash
// exposed the lie: with the tin dumped, free, into the GRAVEL bin, a pan cannot
// concentrate it, because coarse quartz and coarse cassiterite both fall far faster
// than the pan's cut and both stay -- panning the raw heap keeps 98% of the tin at
// UNCHANGED grade. "Free" is not "won." The truth a placer encodes is that the ore
// is not just free but SIZED: a river drops the dense mineral into a heavy-sand band
// a pan does concentrate, and rides the barren rock on as coarse cobble you screen
// off. That is the Origin field: HARDROCK breaks and sizes by the pick's blow;
// PLACER arrives free and pre-classified. With it, the placer wash lifts the tin
// from 45% to ~76% grade with no stone, and the same wash at the copper hill wins
// ~2% -- which is finally the lesson, measured instead of asserted.
//
// AUTHORED, tracked in issue #28: the whole valley layout (deposit centers, radii,
// peak grades -- now capped at COMPOSITE_TARGET_FRACTION), N_TIER, the per-tier
// mineral assignment, and the linear falloff are game-world placeholders -- a human
// chose them, no correct value exists. What is NOT a free knob: the per-mineral
// liberation sizes, the blow's size distribution, and the placer's ore/gangue size
// bands below are grounded in the mineralogy (sources cited inline), and scratchpad
// probes confirmed the findings they produce -- placer tin free AND pan-concentrable
// at the face, hard-rock sulfide locked -- do not depend on the exact splits across
// their plausible ranges. The tests assert only structure (barren-by-default,
// monotone grading, depth-changes-mineral, copper/tin never co-located, placer frees
// and a wash concentrates it while the sulfide waits for the breaker), so correcting
// any layout number changes no test.

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

// How the ore got to where the pick finds it -- and therefore what state it is in
// when it comes up. This is not decoration: it decides whether a wash at the hole
// wins the ore or whether the breaker is the only way in (see THREE THINGS #2).
//
//   HARDROCK -- the ore is still locked in the rock it grew in. A pick BREAKS that
//     rock, and comminution spreads the spoil over a size distribution (GGS); the
//     mineral rides up as composite, freed only in the bins finer than its grain
//     size, locked and breaker-bound in the coarse. The copper hill.
//   PLACER  -- the river already did the breaking. Alluvium is rock a current
//     comminuted and then HYDRAULICALLY SORTED: the dense mineral, weathered free
//     of its gangue, is dropped where the flow slackened and concentrated into a
//     narrow heavy-SAND band, while the barren country rock rode on as coarse
//     cobble and sand. So a placer's ore comes up already FREE and already sized
//     into the fraction a pan concentrates -- no composite, no breaker. This is the
//     mechanical content of "cassiterite concentrates in placers under running
//     water and was won by panning gravel" (phase_table.h). The tin creek.
//   WEATHERED -- the RAIN already did the breaking, chemically. Rotted rock in
//     place (a saprolite): feldspar hydrolysed to kaolinite where it stood, the
//     resistant quartz left behind as loose sand, the whole bank soft enough to
//     crumble by hand at any depth the rot reaches. The mineral comes up FREE --
//     not because a blow parted it but because kaolinite IS the weathering
//     product; it never was locked in anything -- and fine, because kaolinite
//     crystallises at clay size. WEATHERED is to rain what PLACER is to the
//     river: the ground already did the work, and the tool bootstrap is not
//     needed to start. This is load-bearing: the clay bank is where the FIRST
//     vessel is dug (levigate.h HOLLOW, "a hollow scraped in the bank") and the
//     first pan is won, so it must yield to bare hands or the chain deadlocks
//     on its own first rung. The clay bank.
enum Origin { HARDROCK, PLACER, WEATHERED };

struct Deposit {
    const char* id;
    double cx, cy;             // center on the valley floor, m
    double radius;             // m; ore grade fades to background at the rim
    double peak_grade;         // mineral mass fraction at the dead center. Capped
                               // at COMPOSITE_TARGET_FRACTION (0.5): the ore in the
                               // locked fraction is half-mineral composite rock, so
                               // a bite cannot read richer than a solid ore grain.
    int tier_mineral[N_TIER];  // the ore phase at each depth, or BARREN
    Origin origin;             // hard rock (locked, breaker-bound) or placer (free,
                               // pan-ready). Decides the size/liberation of the bite.
};

// The valley. Three bodies, placed far apart so the co-location gate is real: a
// copper hill (oxide cap, leached middle, sulfide root); a few hundred meters
// off, a tin creek (cassiterite, the same at every depth -- no sulfide column);
// and between them on the creek bank, the clay bank the player is GIVEN --
// the ground the first vessel is scraped into and the first pan comes from.
// This is the concrete form of DESIGN.md's "six hand-authored deposit
// compositions"; more can be added the same way, no procedural generation.
inline constexpr Deposit DEPOSITS[] = {
    {"copper-hill", 0.0,   0.0,  40.0, 0.45, {CUPRITE, BARREN, CHALCOCITE}, HARDROCK},
    {"tin-creek",   300.0, 120.0, 25.0, 0.45, {CASSITERITE, CASSITERITE, CASSITERITE}, PLACER},
    // The clay bank -- the creek bank where the rot runs deep, partway down the
    // valley toward the tin creek. Kaolinite through the weathered blanket; the
    // DEEP tier is BARREN because weathering fades and fresh granite is country
    // rock. This is the given first station: the HOLLOW is scraped here, the
    // first wash pan is levigated from its ground. Peak grade 0.35 (saprolite
    // kaolinite content; AUTHORED, UNVERIFIED -- placement and grade are #28
    // valley-layout placeholders like every center/radius in this table).
    {"clay-bank",   150.0, 60.0,  20.0, 0.35, {KAOLINITE, KAOLINITE, BARREN}, WEATHERED},
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
// Wentworth bins and normalised over the four of them. This is FRESH BREAKAGE --
// what a pick does to hard rock (Origin HARDROCK).
inline double psd_bin(int s) {
    const double xmax = SIZE_BOUNDS[GRAVEL][1];
    auto cum = [&](int b) { return std::pow(SIZE_BOUNDS[b][1] / xmax, PSD_MODULUS); };
    const double lo = (s == 0) ? 0.0 : cum(s - 1);
    return cum(s) - lo;
}

// A PLACER is not fresh breakage; it is a hydraulic concentration, and the two
// sides of that sort into DIFFERENT sizes -- which is the whole reason a placer can
// be worked with a pan and no crusher.
//
//   PLACER_ORE_PSD -- the heavy mineral, weathered free and dropped by the current,
//     concentrates in a narrow band of heavy SAND (it settles with, and is trapped
//     among, quartz several sizes coarser -- hydraulic equivalence). Placer
//     cassiterite is a sand-to-fine-granule mineral, worked from the black-sand
//     heavies; the classic sluice/jig feed. SOURCED as a size band, not a game
//     knob (placer cassiterite grain size, e.g. Falconer 2003, Miner. Eng.); a
//     point taken from a real distribution, like bin_diameter -- UNVERIFIED, and
//     the finding it drives (a placer wash concentrates the tin, a hard-rock wash
//     of the same grade does not) is insensitive to the exact split (scratchpad
//     probe). clay, silt, sand, gravel:
inline constexpr double PLACER_ORE_PSD[N_SIZE]    = {0.02, 0.10, 0.78, 0.10};
//   PLACER_GANGUE_PSD -- the barren country rock the current could not lift with
//     the heavies: coarse alluvial cobble and sand, weighted to GRAVEL. It is what
//     you screen off and throw back; that it is coarse is what LETS you screen the
//     heavy sand out from under it.
inline constexpr double PLACER_GANGUE_PSD[N_SIZE]  = {0.02, 0.06, 0.32, 0.60};

// A WEATHERED body is neither fresh breakage nor a hydraulic sort -- it is rock
// rotted IN PLACE, and its two sides part by HOW they weathered, not by a current:
//   WEATHERED_ORE_PSD -- kaolinite crystallises at clay size (booklets under ~4 um;
//     the same mineralogical fact that gives it its platelet aspect ratio,
//     settling.h), so the ore is overwhelmingly in the CLAY bin, with a SILT tail
//     of aggregates the stir does not fully slake. A grain-size fact, not a knob;
//     the split within it is AUTHORED, UNVERIFIED. clay, silt, sand, gravel:
inline constexpr double WEATHERED_ORE_PSD[N_SIZE]    = {0.70, 0.20, 0.08, 0.02};
//   WEATHERED_GANGUE_PSD -- the residual quartz weathering could not touch, freed
//     from its granite as loose sand and grit (saprolite texture: clayey sand).
//     AUTHORED, UNVERIFIED; the finding it drives (a levigation of clay-bank dirt
//     yields kaolinite, the sand stays in the sediment) is insensitive to the
//     split, because the vat cuts on settling TIME and the bins are ~200x apart.
inline constexpr double WEATHERED_GANGUE_PSD[N_SIZE] = {0.05, 0.15, 0.55, 0.25};

// The coarsest size bin at which a mineral's grains come FREE of gangue when HARD
// ROCK is broken -- its liberation size, set by how the mineral occurs in the rock.
// Grains at or finer than this are parted from gangue by the blow itself; coarser
// grains ride up as LOCKED composite the breaker must crush. This governs the
// HARDROCK path only; a PLACER's ore is already free (the river liberated it) and
// bypasses this entirely. These are grain sizes, not game knobs:
//   CASSITERITE -- coarse and weathered free even at GRAVEL. Documented here for a
//     hard-rock (lode) tin vein; the tin CREEK is a placer and never reaches this.
//     (~80% of world tin is placer, won by gravity precisely for this coarse, ready
//     liberation; Falconer, "A review of cassiterite beneficiation," Miner. Eng.
//     2003.)
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

// ---------------------------------------------------------------------------
// THE PICK GATE -- the wall the whole tool staircase was climbed to get under.
//
// Until here a tool only set how MUCH rock a bite won, never whether it could win it
// at all (sample()'s `mass`, dig.cpp's PICK_BITE). That was half the truth. The other
// half is why the easy gossans were worked for an age before the deep sulfides:
// WEATHERING. A weathered oxide CAP is rotten, friable ground a bare hand can scratch;
// the primary sulfide ROOT below is fresh, competent rock a hand skips off. So depth
// is not only a change of MINERAL (THREE THINGS #1) -- it is a rising WALL, and only a
// concentrated blow gets under the deep rock. That blow is haft.h's HEAD_POINT pick:
// knap's floored edge put on haft's lever, delivering FORCE, not keenness (haft.h). The
// tool bootstrap -- gather -> knap -> haft -> the pick -- exists to get back UNDER the
// ground the chain started on, and the primary ore is its reward.
//
// A PLACER needs no pick. Alluvium is loose rock a river already broke and sorted, so
// there is nothing to WIN from the face; a bare hand takes the tin creek whole. This is
// the same asymmetry the breaker shows (a wash wins the placer, the hard rock waits for
// the crush) -- now at the face, before the pan, instead of after it.
//
// rock_competence is AUTHORED (issue #39) on the SAME arbitrary energy scale as
// haft.h's swing_energy, so a tool's bite() compares straight to it -- a named bridge
// (like bin_diameter's UNVERIFIED point), not a measurement. geology.h does NOT include
// haft.h: the ground has no notion of tools. The gate takes a raw `blow_energy`, and
// the caller (a slice, a test) supplies it from hand_bite()/Hafted::bite(). The numbers
// are stand-ins; the finding is the ORDERINGS -- a bare hand wins the cap and the placer
// and skips the root; even a crude sapling pick wins every tier -- asserted against real
// haft energies in test_separation.cpp.
inline constexpr double COMPETENCE_SURFACE = 0.001; // weathered, friable cap: a hand takes it
inline constexpr double COMPETENCE_MIDDLE  = 0.020; // leached, part-weathered: pick ground
inline constexpr double COMPETENCE_DEEP    = 0.100; // fresh unweathered rock: pick only
inline constexpr double COMPETENCE_PLACER  = 0.000; // loose alluvium, already broken: no rock to win

// How hard the rock is to WIN from the face at (origin, tier). Weathering fades with
// depth, so a hard-rock column stiffens SURFACE < MIDDLE < DEEP; a placer is loose all
// the way down.
inline double rock_competence(Origin origin, int tier) {
    if (origin == PLACER) return COMPETENCE_PLACER;
    // Rotted ground is friable as deep as the rot reaches -- the weathering that
    // made the kaolinite also broke the rock's competence. Where the blanket ends
    // (a BARREN tier), origin_at falls through to HARDROCK country and the wall
    // returns. This is the same asymmetry as the placer's, from chemistry instead
    // of a current -- and it is what lets the FIRST vessel be dug with bare hands.
    if (origin == WEATHERED) return COMPETENCE_SURFACE;
    switch (tier) {
        case SURFACE: return COMPETENCE_SURFACE;
        case MIDDLE:  return COMPETENCE_MIDDLE;
        default:      return COMPETENCE_DEEP;
    }
}

// Which Origin governs the rock at (at, tier): the body under this spot if one covers
// it, else HARDROCK country rock (which is fresh, competent rock at depth too). Bodies
// are placed far apart, so at most one covers a spot.
inline Origin origin_at(Place at, int tier) {
    for (const Deposit& d : DEPOSITS) {
        if (d.tier_mineral[tier] == BARREN) continue;
        const double dx = at.x - d.cx, dy = at.y - d.cy;
        if (std::sqrt(dx * dx + dy * dy) < d.radius) return d.origin;
    }
    return HARDROCK;
}

// Does a blow of this energy WIN rock of this competence, or skip off it? A hard
// threshold, like the staircase's other authored gates (fuel tiers, knap's fracture,
// the haft joint): below the wall the point skips and wins NOTHING; at or above it the
// bite comes up. No soft ramp -- the finding is binary: can you get under this rock.
inline bool wins_rock(double blow_energy, double competence) {
    return blow_energy >= competence;
}

// A bite of ground: the makeup at (`at`, `tier`), rendered as `mass` kg of broken
// rock. `mass` is the tool's bite -- bare hands take a little to read the panel, a
// pick wins perhaps ten times as much of the very same makeup. (The shovel is a
// LOADING tool, downstream in the carry loop; it moves the pile a pick has already
// won, it does not win rock from the face.)
//
// What comes up is a SIZE DISTRIBUTION, not one coarse grade, and liberation rides
// on it -- but the state it comes up in depends on how the ore GOT there (Origin):
//
//   HARDROCK. The mineral travels in composite rock (half mineral, half gangue);
//   the pick's blow spreads that rock over the GGS PSD, and every bin at or below
//   the mineral's liberation size comes up already FREE -- grain and gangue parted
//   by the blow -- while coarser bins ride up LOCKED, still composite, still needing
//   the breaker. A disseminated sulfide's bite is rock you must carry and crush.
//
//   PLACER. The river already broke and sorted it: the mineral comes up FREE (no
//   composite) and CLASSIFIED into the heavy-sand band (PLACER_ORE_PSD), the barren
//   country rock riding as coarse cobble (PLACER_GANGUE_PSD). A placer's bite is
//   pan-ready at the face -- screen off the cobble, wash the sand, and the density
//   difference does the rest, no breaker touched.
//
// Either way the panel reads the true GRADE (mass is conserved and the free/locked
// split does not touch it); what changed is whether that grade is trapped behind
// the crusher or waiting in the pan.
inline Substance sample(Place at, int tier, double mass) {
    constexpr double f = COMPOSITE_TARGET_FRACTION;
    Substance s;
    double consumed = 0.0;         // bite mass bound up in ore-bearing rock
    Origin barren_origin = HARDROCK; // which size the leftover country rock takes
    for (const Deposit& d : DEPOSITS) {
        const int mineral = d.tier_mineral[tier];
        if (mineral == BARREN) continue;
        const double dx = at.x - d.cx, dy = at.y - d.cy;
        const double dist = std::sqrt(dx * dx + dy * dy);
        const double falloff = dist < d.radius ? 1.0 - dist / d.radius : 0.0;
        const double richness = d.peak_grade * falloff;
        if (richness <= 0.0) continue;
        barren_origin = d.origin;  // bodies are placed apart, so one bite, one body
        if (d.origin == PLACER) {
            // Free heavy grains, river-classified into the heavy-sand band. The
            // grade IS the richness -- no locked gangue rides with it.
            const double ore = mass * richness;
            for (int b = 0; b < N_SIZE; ++b)
                s.freegrain[mineral][b] += ore * PLACER_ORE_PSD[b];
            consumed += ore;
        } else if (d.origin == WEATHERED) {
            // Rotted in place: the mineral was MADE free by chemistry (kaolinite
            // is the weathering product, never locked in anything) and comes up
            // fine; the residual quartz rides with it as loose sand. Both free,
            // no composite, no breaker -- the rain was the mill.
            const double ore = mass * richness;
            for (int b = 0; b < N_SIZE; ++b)
                s.freegrain[mineral][b] += ore * WEATHERED_ORE_PSD[b];
            consumed += ore;
        } else {
            // It takes richness/f of composite rock to carry `richness` of mineral;
            // the blow spreads that rock over the GGS PSD and liberation splits it.
            const double rock = mass * richness / f;
            const int lib = liberation_bin(mineral);
            for (int b = 0; b < N_SIZE; ++b) {
                const double here = rock * psd_bin(b);
                if (b <= lib) {
                    s.freegrain[mineral][b] += f * here;           // freed by the blow
                    s.freegrain[GANGUE][b]  += (1.0 - f) * here;   // its gangue too
                } else {
                    s.composite[mineral][b] += here;               // still locked rock
                }
            }
            consumed += rock;
        }
    }
    if (consumed > mass) consumed = mass; // deposits placed apart, so this never
                                          // bites -- it only floors the gangue term.
    // Everything not bound up in ore-bearing rock is barren country rock -- free
    // quartz, coarse. Fresh breakage at a hard-rock body (GGS), coarse alluvial
    // cobble at a placer (PLACER_GANGUE_PSD). This is why most of the valley reads
    // as nothing on the panel, and why even a rich bite is mostly waste.
    const double gangue = mass - consumed;
    for (int b = 0; b < N_SIZE; ++b)
        s.freegrain[QUARTZ][b] += gangue *
            (barren_origin == PLACER    ? PLACER_GANGUE_PSD[b]
           : barren_origin == WEATHERED ? WEATHERED_GANGUE_PSD[b]
                                        : psd_bin(b));
    return s;
}

// A full-depth dig at one spot: the pick crosses every tier and the spoil pile
// MIXES them. At the hard-rock copper hill this is the mixed column -- oxide from
// the cap AND sulfide from the root, plus the barren middle, all heaped together:
// the richer haul and the harder problem at once, since the little that frees at the
// face can be panned on the spot but the locked coarse must be CRUSHED to free its
// grains and PANNED to part ore from waste (separate.h crush, then separate) before
// the furnace -- and the concentrate still carries both minerals, because the oxide
// smelts straight and the sulfide must first be roasted. At a placer the column is
// uniform and already free: every tier is the same pan-ready heavy sand, and the
// crush step drops out entirely. `mass` is the bite taken from each tier.
inline Substance dig_column(Place at, double mass) {
    Substance pile;
    for (int t = 0; t < N_TIER; ++t) pile.add(sample(at, t, mass));
    return pile;
}

// What a TOOL of the given `blow_energy` can take out of one tier. sample() is what the
// ground HOLDS; this is what the tool can WIN of it. If the blow cannot get under the
// rock (wins_rock false) the point skips off and nothing comes up. Otherwise the full
// bite. The panel still reads the true grade of what DID come up -- the tool changed
// REACH, not makeup (sample() is unchanged; a hand and a pick that BOTH win a tier win
// the very same makeup, the old finding, still true).
inline Substance win_bite(Place at, int tier, double mass, double blow_energy) {
    if (!wins_rock(blow_energy, rock_competence(origin_at(at, tier), tier)))
        return Substance{};                       // skipped off the rock -- nothing won
    return sample(at, tier, mass);
}

// A full-depth dig with a tool, tier by tier gated on its blow_energy. A bare hand at
// the copper hill therefore comes up with the weathered oxide CAP and no deep sulfide
// at all -- the root stays in the ground until a pick reaches it; the same column with
// a pick brings oxide AND sulfide, the mixed haul (dig_column above, which is the
// unlimited-reach geological truth this gates). `mass` is the bite per tier.
inline Substance dig_column(Place at, double mass, double blow_energy) {
    Substance pile;
    for (int t = 0; t < N_TIER; ++t) pile.add(win_bite(at, t, mass, blow_energy));
    return pile;
}

} // namespace wrought
