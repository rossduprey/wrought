#pragma once

#include "haft.h"   // the Hafted tool that does the work, and the HaftStock it produces.

// Dressing: a felled trunk into a haft, and why it takes the OTHER tool.
//
// haft.h fells the tree and stops on a convenient lie. The instant the trunk comes
// down it hands you TIMBER -- "the seasoned haft stock every tool after is cut from" --
// as if a standing tree and a finished handle were one thing separated only by the
// swing. They are not. A felled trunk is a green log: round, wet, and running with
// lengthwise grain. You cannot bind a head to it, and if you rushed a billet out of it
// green and crosscut it would snap the first hard swing. Between the trunk on the ground
// and the haft in the hand is a whole second verb, and this file is it:
//
//   THE AXE WINS THE TRUNK; THE ADZE MAKES THE HAFT. SAME EDGE, HAFTED THE OTHER WAY.
//
//   Felling and dressing are opposite geometries of the one knapped edge. An axe sits
//   EDGE_ACROSS -- square to the swing -- to sever the standing grain: that is FORCE,
//   the lever's bite topping the fell wall (haft.h). It is exactly the wrong tool for
//   the next job. To true a face you must pare ALONG the grain, in controlled bites that
//   follow the wood, and that is the adze: EDGE_INLINE, the edge laid in line with the
//   haft so it skims the surface instead of chopping across it. The axe cannot do it --
//   its edge crosses the grain it needs to follow -- so the bootstrap does not turn on
//   one tool. It turns on the same stone edge hafted TWICE: as an axe to bring the trunk
//   down, as an adze to dress it up. Knap once, haft twice.
//
//   And the adze answers to a different gate than the axe. Felling is force: the sapling
//   axe fells only because its bite clears the wall. Dressing is not -- it is the EDGE,
//   a keen controlled paring, and it needs no fell-wall of energy behind it. So the very
//   same crude kit that felled the trunk dresses it: a sapling adze, no timber required,
//   pares the log true. Force needs the lever; the edge does not. That is why the first
//   haft can exist at all -- if dressing wanted timber it would want a tree it had no
//   tool to fell, and the staircase would have no bottom step.
//
// The finding the whole subsystem was climbing toward closes here, and it closes on the
// same shape as every rung below it. knap.h's edge is floored -- a stone edge is only so
// keen -- so the FACE a stone adze leaves is only so true; a metal adze (forge.h, Era 2)
// dresses finer than any stone one can. But true ENOUGH is the whole bar for a haft: a
// well-riven, adze-dressed billet seats a head completely (haft.h's stock_hold 1.0), and
// the stone floor caps the finish, not the hold. Effort does not get under the floor; the
// next material does -- the staircase, one rung further along than knap or haft could say.
//
// Scope (design decision, 2026-07-12): a trunk is not a phase_table.h grain and not
// fuel.h's Wood matrix -- like knap.h's Stone and haft.h's HaftStock, it is its own small
// stock, carried by the one property that gates (its lengthwise grain). Riving and paring
// are collapsed into ONE authored transform, the way knap() collapses many removals into
// one strike: the SIGN is the finding -- an adze along the grain wins a haft, an axe or a
// crosscut does not -- not any retained fraction. No number is authored here; dressing
// either upgrades the sapling to timber or leaves you on the sapling, and the tests read
// only that ordering (through haft.h's joint, which the timber lifts and the sapling caps).

namespace wrought {

// ---------------------------------------------------------------------------
// The felled trunk -- the axe's product, and NOT yet a haft. Green, round, its grain
// running the length of it. A raw log you cannot bind a head to until it is dressed.
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

// Can this tool dress wood? Only a SOUND ADZE. The inline edge (EDGE_INLINE) pares a face
// true along the grain; an axe's edge sits across the swing -- it severs grain, it cannot
// follow it; a pick concentrates to a point; and a loose or edgeless head (sound == false)
// has nothing to pare with. Note there is no force threshold -- dressing is edge-work, so a
// sapling adze qualifies exactly as a timber one does. Force fells; the edge dresses.
inline bool can_dress(const Hafted& tool) {
    return tool.sound && tool.kind == ADZE;
}

// Dress a felled trunk into a haft billet. Two gates, both about doing the RIGHT thing to
// the wood rather than doing it hard: the TOOL must be an adze that holds (geometry), and
// the billet must be riven ALONG the grain. Pass both and the green log yields seasoned
// TIMBER -- the stock that seats a joint the springy sapling never can. Fail either and you
// have won no haft: you are still on the hand-cut SAPLING you started the axe with.
inline HaftStock dress(const Trunk& trunk, const Hafted& adze, Rive rive) {
    if (trunk.mass <= 0.0)   return SAPLING;   // no log to work -- nothing dressed
    if (!can_dress(adze))    return SAPLING;   // wrong tool: an axe fells, it does not dress
    if (rive != ALONG_GRAIN) return SAPLING;   // short grain -- a stick that snaps, no better than a sapling
    return TIMBER;                             // riven with the grain and pared true: the real haft
}

} // namespace wrought
