#pragma once

#include "knap.h"   // the StoneEdge that becomes the head; the staircase this continues.

// Hafting: a struck edge and a worked stick become the first real tool, and the two
// things that decide whether it is one.
//
// knap.h made an EDGE and stopped at a wall: a stone edge is only ever so keen, and
// no strike gets under that floor. This file is the rung that does not TRY to. It
// takes the same floored edge and does the other thing a tool can do -- it puts the
// edge on a LEVER. The gain from here is not sharpness; it is FORCE. And that is the
// whole finding, in two halves:
//
//   THE HAFT IS THE FORCE MULTIPLIER -- AND THE JOINT IS THE CEILING.
//
//   1. THE LEVER. A flake in the fist is knap's edge with almost no handle: your wrist
//      is the lever arm, a few centimetres, so the hand delivers a floor of work no
//      matter how keen the edge. A haft is the length the hand does not have. The tip
//      of a lever moves faster the longer it is, and a blow's energy grows with the
//      SQUARE of that speed -- so lengthening the handle multiplies the bite far faster
//      than it lengthens the handle. This is why a hafted axe fells the timber gather's
//      bare hands could not touch, and why it does so with an edge that never got
//      keener: the tree yields to ENERGY, not to a razor. Hafting buys the axis knap
//      could not -- force -- and leaves knap's floor exactly where it was.
//
//   2. THE JOINT. A lever transmits only what its pivot can hold. The head-haft joint
//      -- lashing, a socket, a wedge -- is the weakest link in the tool, and it, not
//      the head and not the handle, sets the ceiling. A perfect edge on a long haft
//      delivers NOTHING through a joint that lets the head walk off on the first blow.
//      So the tool is only as good as its worst coupling, and the finding is the same
//      shape the whole project keeps finding -- the wrought bar gated by its stringers,
//      the recovery gated by the crush -- a chain answering to its weakest part, forced
//      by assembly rather than authored.
//
// And this is where the circle the whole subsystem was dug to close finally closes.
// A haft is worked wood, and fuel.h locked the good wood -- TIMBER -- behind the axe.
// So the FIRST axe cannot have a timber haft; it can only have a hand-cut SAPLING, the
// green stick gather DID let you win. That first axe is therefore crude by necessity --
// lashed to a springy sapling, its joint capped low -- yet it is just enough to fell
// one tree, and that tree's timber is the seasoned haft stock for every tool after.
// The bootstrap turns exactly once here: hands -> stone edge -> sapling axe -> the
// felled trunk -> the real haft. Tool #1 makes tool #2 (DESIGN.md, the tool bootstrap).
//
// Scope (design decision, 2026-07-12): like fuel.h's fire tiers and knap.h's strike,
// the joint is an authored gate with a few discrete settings, not a fracture-mechanics
// solve, and the swing energy is a RELATIONSHIP in arbitrary units, not joules -- the
// unit is meaningless and no test reads a magnitude, only the orderings (longer/heavier
// hits harder; the weakest link caps it; the edge floor is untouched). The head is
// knap's product whether it is a flake knife or a core worked to a biface; haft() reads
// only its mass, its edge angle, and whether it is a usable edge at all.

namespace wrought {

// ---------------------------------------------------------------------------
// The handle stock -- split by what it was cut from, because that is the gate the
// bootstrap turns on (like fuel.h's Wood by how it is won, like knap's Fracture).

enum HaftStock {
    SAPLING,   // a hand-cut green stick (gather's sticks): the ONLY haft before the axe. Springy, splits.
    TIMBER     // seasoned, split from a felled trunk: straight, stiff, strong. Needs the axe -- comes AFTER.
};

// The coupling between head and haft -- the weakest link, and an authored gate with a
// few discrete settings ordered by how much of a swing the joint survives before it
// fails (like the pit's seal, like the knapper's strike).
enum Bind {
    BIND_NONE,     // head merely held to the haft: it walks off on the first blow. Not a tool.
    BIND_LASHED,   // wrapped with cordage/sinew: quick, but works loose under load.
    BIND_SEATED    // set into a split or socket AND lashed: the joint the head cannot walk out of.
};

// How the head sits on the haft -- the geometry that decides which tool it is. The
// mechanics (lever, joint) are the same for all three; only the WORK it is for changes.
enum Helve {
    EDGE_ACROSS,   // edge square to the swing: an AXE -- bites across the grain, fells and chops.
    EDGE_INLINE,   // edge in line with the haft: an ADZE -- hollows and dresses wood (shapes the next haft).
    HEAD_POINT     // a point, not an edge: a PICK -- concentrates the blow to split the hard rock a dig only scratched.
};

enum ToolKind { AXE, ADZE, PICK };

// ---------------------------------------------------------------------------
// The authored numbers. Each is a RELATIONSHIP, not a measurement (issues #36-#38).

// How much of a swing a joint transmits, per bind and per stock. AUTHORED placeholders
// (issue #36 bind, #37 stock). The joint fails at whichever link gives first, so the
// coupling strength is the MIN of the two -- a perfect lashing on a splitty sapling is
// still a sapling joint; a stiff timber haft with no binding still falls off. The
// ordering (SEATED>LASHED>NONE, TIMBER>SAPLING) is the finding; the fractions are not.
inline double bind_hold(Bind b)        { return b == BIND_SEATED ? 1.00 : b == BIND_LASHED ? 0.55 : 0.0; }
inline double stock_hold(HaftStock s)  { return s == TIMBER ? 1.00 : 0.60; }
inline double joint_strength(Bind b, HaftStock s) {
    const double h1 = bind_hold(b), h2 = stock_hold(s);
    return h1 < h2 ? h1 : h2;   // the weakest link
}

// A swing's energy: head mass times the SQUARE of the haft length. The tip speed of a
// lever grows with its length and energy with speed squared, so the handle multiplies
// the blow faster than it lengthens. AUTHORED as a relationship in arbitrary units; no
// test reads its magnitude, only that more mass and more length hit harder.
inline double swing_energy(double head_mass, double haft_len) {
    return head_mass * haft_len * haft_len;
}

// The bare edge in the fist -- knap's flake, no haft. The grip is a coupling that never
// slips (hold 1.0) but a lever of almost no length. So the hand delivers the FLOOR, and
// the haft's whole gift is the reach the hand lacks. AUTHORED floor arm (issue #38).
inline constexpr double HAND_REACH = 0.10; // m
inline double hand_bite(const StoneEdge& head) {
    return swing_energy(head.mass, HAND_REACH); // joint 1.0: your grip holds, your reach does not
}

// The energy it takes to fell standing timber -- the wall gather ran into. AUTHORED
// (issue #38). Only its ORDERING is asserted, against the fixture: above the bare
// hand (so hands never fell -- gather's finding) and below even the crude sapling axe
// (so the bootstrap turns). The number is a stand-in; the two inequalities are the point.
inline constexpr double FELL_ENERGY = 0.02; // arbitrary units, same scale as swing_energy

// ---------------------------------------------------------------------------
// The hafted tool. NOT forge.h's Tool (that is a shaped metal bar with a hardness); this
// is a stone head lashed to a stick -- the Era-0 implement, the rung below metal.

struct Hafted {
    ToolKind kind = AXE;
    double   head_mass = 0.0;   // kg, the bit
    double   edge_angle = 0.0;  // deg, inherited from the head -- knap's floor, unchanged by hafting
    double   haft_len = 0.0;    // m, the lever arm
    double   joint = 0.0;       // 0..1, the weakest-link coupling strength -- the ceiling
    bool     sound = false;     // a working tool, or a bundle that comes apart on the first swing

    // Work delivered at the edge: the swing the lever wins, times the fraction the joint
    // transmits. A failed joint (0) delivers nothing however keen the head or long the haft.
    double bite() const { return swing_energy(head_mass, haft_len) * joint; }
    // Does it clear the wall gather could not? A sound tool whose bite tops the fell floor.
    bool fells() const { return sound && bite() >= FELL_ENERGY; }
};

// Haft a struck edge to a worked stick. Two gates: the head must be a USABLE edge (a dud
// flake hafts into a club, not an axe -- knap's own gate carried forward), and the JOINT
// must hold (a head that walks off the haft is no tool). Pass both and the head geometry
// names the tool. Hafting adds force, never sharpness: edge_angle passes through untouched.
inline Hafted haft(const StoneEdge& head, double haft_len, HaftStock stock, Bind bind, Helve helve) {
    Hafted t;
    t.head_mass  = head.mass;
    t.edge_angle = head.edge_angle;   // knap's floor, unchanged -- hafting is force, not keenness
    t.haft_len   = haft_len;
    t.kind = helve == HEAD_POINT ? PICK : helve == EDGE_INLINE ? ADZE : AXE;

    if (!head.usable) return t;                 // no edge, no tool: sound stays false, joint 0
    t.joint = joint_strength(bind, stock);
    t.sound = t.joint > 0.0;                     // a joint that cannot hold is not a tool
    return t;
}

} // namespace wrought
