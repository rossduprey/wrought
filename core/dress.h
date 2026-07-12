#pragma once

#include "season.h"   // the seasoned Stave it trues; split.h's Trunk/Rive ride in through here.

// Dressing: a seasoned billet into a haft, and why it takes the OTHER tool -- the adze.
//
// This file once did three jobs and claimed one verb for all of them. It took a raw green
// Trunk and, in a single adze pass, RIVED it (chose the grain), DRIED it ("seasoned
// TIMBER"), and trued it -- as if the adze split the log and ran the calendar. It does
// neither of the first two. Riving is a wedge and the grain (split.h); seasoning is the
// clock (season.h); and what is left for the adze -- the only thing that was ever truly
// its verb -- is this: to true the seat. That is the honest scope of dressing now.
//
//   THE AXE WINS THE TRUNK; THE ADZE TRUES THE HAFT. SAME EDGE, HAFTED THE OTHER WAY.
//
//   Between the axe's two turns sit two edgeless stations -- the wedge that splits and the
//   time that seasons -- and then the edge returns. A riven, seasoned billet is stiff and
//   straight but its bearing face is rough and round off the wedge; the head cannot bed on
//   it. Truing that face is paring ALONG the grain in controlled bites, and that is the
//   adze: EDGE_INLINE, the edge laid in line with the haft so it skims the surface instead
//   of chopping across it. The axe cannot do it -- its edge crosses the grain it must
//   follow -- so the bootstrap still turns on the same stone edge hafted TWICE: an axe to
//   bring the trunk down, an adze to true the billet up. Knap once, haft twice. What has
//   changed is only that the two edgeless verbs between them are no longer the adze's to
//   claim -- force fells, the wedge splits, time seasons, the edge trues.
//
//   And this is where the seasoning earns its keep mechanically. dress() hands back real
//   TIMBER -- the stock that seats a joint at full hold (haft.h's stock_hold 1.0) -- only
//   for a billet that is BOTH riven along the grain (split.h: sound) AND dried to air-dry
//   (season.h: is_seasoned). Present it a green billet and the adze trues a face that will
//   shrink out from under the head by autumn: you are handed back the SAPLING, no better a
//   hold than the springy stick you started with. The calendar is a gate the tool cannot
//   pass for you.
//
// The finding the subsystem climbed toward still closes here on the staircase's shape:
// knap.h's edge is floored, so the FACE a stone adze trues is only so true; a metal adze
// (forge.h, Era 2) dresses finer. But true ENOUGH seats a head completely, and the stone
// floor caps the finish, not the hold. Effort does not get under the floor; the next
// material does.
//
// Scope (design decision, 2026-07-12): dressing authors no number. Its whole content is
// the SIGN -- a sound adze trues a sound, seasoned billet into TIMBER, and nothing else
// does -- read through the gates split.h and season.h already set on the billet, and
// through haft.h's joint, which the timber lifts and the sapling caps.

namespace wrought {

// Can this tool dress wood? Only a SOUND ADZE. The inline edge (EDGE_INLINE) pares a face
// true along the grain; an axe's edge sits across the swing -- it severs grain, it cannot
// follow it; a pick concentrates to a point; and a loose or edgeless head (sound == false)
// has nothing to pare with. Dressing is edge-work, not force, so a sapling adze qualifies
// exactly as a timber one does. Force fells; the wedge splits; time seasons; the edge trues.
inline bool can_dress(const Hafted& tool) {
    return tool.sound && tool.kind == ADZE;
}

// Dress a seasoned billet into a haft. Three gates, and the tool passes only the last: the
// billet must be riven along the grain (split.h set sound), it must have dried to air-dry
// (season.h: green wood loosens), and the TOOL must be an adze that holds, to true the seat.
// Pass all three and the billet is TIMBER -- the stock that seats a joint the springy
// sapling never can. Miss any and you have won no haft: you are back on the SAPLING.
inline HaftStock dress(const Stave& billet, const Hafted& adze) {
    if (!billet.sound)        return SAPLING;   // riven across the grain -- short-grained waste, no haft
    if (!is_seasoned(billet)) return SAPLING;   // still green -- it will loosen as it dries, not timber yet
    if (!can_dress(adze))     return SAPLING;   // wrong tool: an axe fells, it does not true a face
    return TIMBER;                              // along the grain, seasoned to air-dry, adze-trued: the haft
}

} // namespace wrought
