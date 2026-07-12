#pragma once

#include "fuel.h"   // for the namespace and the staircase it sits on; no code shared.

// Knapping: loose stone into a cutting edge, and the two things that gate it.
//
// gather.cpp ended at the wall the whole staircase leans on -- hands win sticks and
// stone but never timber, because felling wants an axe, the axe wants a hafted edge,
// and the edge wants a KNAPPED STONE. This file is that first edge. It is the rung
// below every pick a dig assumes and every haft a tool is cut from: the one tool you
// can make with nothing but your hands and two rocks.
//
// gather handed you a mass of "loose stone" and called it the seed of the other road.
// It lied by omission, and this file collects the debt, because the first fact of
// knapping is that the stone you pick up is the WRONG stone:
//
//   FRACTURE, NOT HARDNESS. AND THE EDGE STONE IS NOT THE STONE UNDERFOOT.
//
//   An edge is a fresh fracture surface, and only one kind of stone fractures the way
//   an edge needs. Flint, chert, obsidian, fine quartzite -- brittle, isotropic, no
//   grain and no cleavage -- break CONCHOIDALLY: in smooth shell-shaped shells that
//   run predictably from the blow and leave an edge keener than steel where two scars
//   meet. Ordinary cobble -- sandstone, granite, most of what is lying around -- is
//   GRANULAR: struck, it powders or splits blocky along its grains, and takes no edge
//   at all. So the loose stone gather let you pocket is a HAMMERSTONE, not a blade:
//   the tough rock that survives the blow is exactly the rock that cannot receive one.
//   The two roles want opposite stones, and the core -- the conchoidal cobble -- is a
//   thing you must FIND (a flint nodule, a river chert), not a thing you trip over.
//
// And knapping is the project's oldest law wearing stone: it AUTHORS NOTHING. Like
// levigation, like the wrought bar, the edge is not added -- it is what remains when
// everything that is not the edge has been struck away. Every blow removes mass to
// spalls and debris; a flake is subtraction, and it conserves (flake + core-after +
// debris = the stone you started with). You cannot knap sharpness INTO a rock, only
// out of it, and you can take away too much: overstrike and the core shatters, mass to
// gravel and no edge to show.
//
// Which is why the edge has a FLOOR it cannot beat, and the floor is the staircase
// itself (DESIGN.md, the tool bootstrap: "its own irreducible sharpness"). A stone
// edge is brittle, so a bit thin enough to be truly keen chips away the first time it
// bites; a working stone edge must stay BLUNT enough to survive use. That angle is a
// wall no skill or patience gets under -- keen enough to cut a haft and skin a kill,
// never keen enough to be a razor. The rung that gets under it is not a better-knapped
// stone; it is metal (forge.h, Era 2), which re-hafts the same handles with an edge
// this file's stone can never hold. Effort does not reach the floor; the next material
// does. That is the staircase, restated one rung lower than it has ever been stated.
//
// Scope (design decision, 2026-07-12): a knappable core is NOT a phase_table.h grain,
// for the same reason fuel.h keeps Wood out of it -- a flint nodule is not a Wentworth
// particle, and forcing it into the mineral size matrix would be a lie of convenience.
// Stone here is its own small stock, split by how it FRACTURES (the property that
// gates), just as Wood is split by how it is WON. The strike is an authored gate with
// a few discrete settings, not a computed fracture-mechanics solve; the SIGN is the
// finding -- acute detaches, steep hinges, hard shatters -- not the retained fractions.

namespace wrought {

// ---------------------------------------------------------------------------
// Stone, as picked up -- split by the one property that decides an edge.
//
// Not by mineralogy: this model has no use for "granite vs sandstone," only for
// "breaks the right way vs the wrong way." CONCHOIDAL is the core; GRANULAR is the
// hammer -- and gather.cpp's loose stone is GRANULAR, the tough kind that survives a
// blow, which is exactly why it can never receive one.

enum Fracture {
    CONCHOIDAL,  // brittle, isotropic, no grain: flint/chert/obsidian. Flakes; carries the edge.
    GRANULAR     // grainy or cleaved: sandstone/granite/loose cobble. Powders; a hammer, never a blade.
};

struct Stone {
    double   mass = 0.0;             // kg
    Fracture fracture = GRANULAR;    // gather's loose stone defaults here -- a hammerstone.
};

// ---------------------------------------------------------------------------
// The blow -- an authored gate, like the pit's seal (char.h). The one decision the
// station makes, in a few discrete settings, not a fracture-mechanics solve.

enum Strike {
    STRIKE_ACUTE,  // struck proud of the platform, below 90 deg: a flake feathers off clean, edge at the floor.
    STRIKE_STEEP,  // struck too square: the force sinks in, the flake hinges off short -- mass lost, no usable edge.
    STRIKE_HARD    // overpowered: the core shatters -- all to debris, no flake, no edge.
};

// Fraction of the core a single clean flake carries off. AUTHORED placeholder (issue
// #34): knapping is many small removals, not one; the SIGN -- a flake is a small
// bite, most of the core survives the blow -- is the point, not the fraction.
inline constexpr double FLAKE_FRACTION = 0.08;

// The floor a stone edge cannot beat: the keenest bit angle it holds without chipping
// away in use. AUTHORED, UNVERIFIED (issue #35). ~45 deg stands for "blunt enough to
// survive, keen enough to cut" -- a stone axe/adze bit. A metal edge (forge.h) goes
// well below this; no knapping reaches it. The NUMBER is a stand-in; the WALL is the
// finding. Smaller angle = keener edge.
inline constexpr double STONE_EDGE_FLOOR = 45.0; // degrees

// The struck flake -- the blade, or the spalls that were not one.
struct StoneEdge {
    double mass = 0.0;        // kg of the detached flake
    double edge_angle = 0.0;  // deg; the bit angle, floored by the stone (smaller is keener)
    bool   usable = false;    // did a real cutting edge come off, or only debris
};

// What a blow leaves: the flake, the core that remains, and the mass lost to debris.
// Mass conserves -- flake.mass + core.mass + debris == the core you struck.
struct KnapResult {
    StoneEdge flake;
    Stone     core;          // the core after the blow (fracture unchanged; mass reduced)
    double    debris = 0.0;  // kg powdered/shattered away -- mass, never an edge
};

// Strike a core with a hammerstone. Two gates, both material before the blow even
// lands: the CORE must be CONCHOIDAL (a granular core just powders -- no edge at any
// strike), and the HAMMER must be GRANULAR-tough (a brittle hammer shatters itself on
// contact). Pass both, and the STRIKE decides what comes off. A clean edge is only
// ever as keen as STONE_EDGE_FLOOR -- the wall the next material, not this one, gets
// under.
inline KnapResult knap(const Stone& core, const Stone& hammer, Strike strike) {
    KnapResult r;
    r.core = core;

    // Gate 1 (material): wrong stones, no edge -- and you feel it in your palm.
    // A granular core powders under the blow; a brittle hammer shatters on the core.
    // Either way the core is untouched as a blade and a little mass crumbles to grit.
    if (core.fracture != CONCHOIDAL || hammer.fracture != GRANULAR) {
        const double grit = core.mass * FLAKE_FRACTION;   // knocked loose, but shapeless
        const double lost = grit < core.mass ? grit : core.mass;
        r.core.mass -= lost;
        r.debris = lost;
        return r;                                          // flake.usable stays false
    }

    // Gate 2 (the blow): the same right stones give an edge, a dud, or a ruin.
    const double bite = core.mass * FLAKE_FRACTION;
    const double take = bite < core.mass ? bite : core.mass;
    switch (strike) {
        case STRIKE_ACUTE:
            // A flake feathers off clean: it carries the edge, at the stone's floor.
            r.flake.mass = take;
            r.flake.edge_angle = STONE_EDGE_FLOOR;
            r.flake.usable = true;
            r.core.mass -= take;
            r.debris = 0.0;                                // clean removal, nothing wasted
            break;
        case STRIKE_STEEP:
            // The flake hinges off short: mass leaves, but blunt and stubby -- no edge.
            r.flake.mass = take;
            r.flake.edge_angle = 0.0;                      // no cutting edge formed
            r.flake.usable = false;
            r.core.mass -= take;
            r.debris = 0.0;
            break;
        case STRIKE_HARD:
            // Overpowered: the core bursts. Mass to debris, and no flake to keep.
            r.debris = take;
            r.core.mass -= take;
            break;
    }
    return r;
}

} // namespace wrought
