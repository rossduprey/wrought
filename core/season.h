#pragma once

#include <cmath>    // std::exp -- the diffusion clock
#include "split.h"  // the green Stave it dries, and the thickness that sets the clock.

// Seasoning: a green billet into stiff dry haft stock, and why the gate is the CALENDAR.
//
// dress.h called a freshly-worked billet "seasoned TIMBER" the instant the adze touched
// it -- as if drying were a stroke of the tool. It is not a verb at all. It is TIME, and
// this file is the first station in the whole project whose gate is not force, not an
// edge, not geometry, but the clock:
//
//   GREEN WOOD IS NOT A HAFT. IT LOOSENS AS IT DRIES -- THE JOINT FAILS AT THE CALENDAR.
//
//   A billet off the wedge is soaking -- three-quarters of its weight is water (GREEN_MC).
//   Water leaves in two phases, and only the second one matters. Above the FIBRE
//   SATURATION point the FREE water in the cell cavities leaves first, and the wood does
//   not move or stiffen at all -- it is lighter, no better. Only BELOW fibre saturation
//   does bound water leave the cell walls, and THEN the wood shrinks and gains its
//   stiffness and strength. So a haft bound to green wood does not fail on the swing
//   (haft.h's joint gate) -- it fails on the calendar: as the billet dries the wood
//   shrinks out from under the lashing and the head, seated tight in spring, walks loose
//   by autumn. Seasoning is what earns the timber's full hold; skip it and the split
//   billet seats no better than the springy sapling it was supposed to replace.
//
//   And the clock runs on a diffusion square, which is why split.h split thin FIRST.
//   Water crawls out by Fickian diffusion, so the time to dry grows with the SQUARE of
//   the billet's thickness (tau ~ L^2/D). A thin riven billet air-dries in a season; a
//   billet twice as thick takes four times as long; the whole round log takes a lifetime
//   and is the reason you never season the log -- you rive it thin and season the billets.
//   The trade's "about a year per inch of thickness to air-dry" is the rule-of-thumb
//   bridge to that square; the value here is set to sit near it (issue #41, #42).
//
// Seasoning removes, it does not add: like knapping and splitting, nothing is authored
// INTO the wood -- water leaves, and what remains is stiffer because less of it is water.
// The ceiling is not zero moisture but EQUILIBRIUM: air-drying stops at the air-dry EMC,
// the moisture the wood holds in balance with sheltered outdoor air; a kiln (a later era)
// goes below it, air-drying never does. Effort does not get under that floor; a hotter,
// drier process does -- the staircase, restated for water instead of an edge.
//
// Scope (design decision, 2026-07-12): the drying curve is an authored diffusion
// relaxation, not a coupled heat-and-mass-transfer solve -- the SIGN is the finding
// (moisture falls toward EMC; thicker is slower by a square; nothing stiffens until below
// fibre saturation), and the tests read only those orderings, never a moisture magnitude.
// The moisture thresholds (fibre saturation, air-dry EMC) are looked-up wood-science
// numbers shipped UNVERIFIED (issue #41); the drying rate is a stand-in near the trade
// rule (issue #42). GREEN_MC, the start of the curve, lives with the log in split.h (#40).

namespace wrought {

// The moisture content, dry basis, at which the cell walls are saturated: above it only
// free water leaves (no shrinkage, no stiffening); below it the wood shrinks and gains
// strength. UNVERIFIED (issue #41); wood-science value ~28-31%, 0.30 stands in.
inline constexpr double FIBER_SATURATION = 0.30;

// The equilibrium moisture the wood settles to in balance with sheltered outdoor air --
// the floor air-drying reaches and cannot beat (a kiln goes lower). UNVERIFIED (issue
// #41); ~12-18% depending on climate, 0.15 stands in.
inline constexpr double AIR_DRY_EMC = 0.15;

// The drying time constant per unit of thickness-squared, in years per mm^2. Diffusion
// clock: tau = SEASON_TAU_MM2 * (thickness in mm)^2, and the billet reaches air-dry after
// a few tau. Set to sit near the trade rule (~1 yr / 25 mm to air-dry). UNVERIFIED /
// AUTHORED bridge (issue #42): the SQUARE is the physics (Fickian L^2/D), the coefficient
// is a stand-in tuned to the rule of thumb.
inline constexpr double SEASON_TAU_MM2 = 5.0e-4;

// How close to the air-dry EMC counts as "seasoned" -- a billet approaches equilibrium
// asymptotically and never reaches it exactly, so "air-dry" is a band just above EMC.
// A modelling convention (like a fixed-point tolerance), not a measured property; stated,
// not looked up. 1.10 = within 10% of equilibrium.
inline constexpr double SEASONED_BAND = 1.10;

inline constexpr double SEASON_SPECK = 1e-9;

// The stiffness a billet has EARNED, 0..1: none at all while it is still above fibre
// saturation (free water leaving changes nothing), then rising as bound water leaves and
// the wall shrinks, full when it reaches air-dry. This is what a green billet lacks and a
// seasoned one has -- the reason the joint holds by autumn only if the wood dried first.
inline double seasoned_fraction(const Stave& b) {
    if (b.moisture >= FIBER_SATURATION) return 0.0;   // still soaking above FSP: no stiffening
    const double span = FIBER_SATURATION - AIR_DRY_EMC;
    const double f = (FIBER_SATURATION - b.moisture) / span;
    return f < 0.0 ? 0.0 : f > 1.0 ? 1.0 : f;
}

// Has the billet dried to air-dry -- the haft-ready state dress.h will demand? A sound
// billet whose moisture has come down within the air-dry band. A green or waste billet is
// never seasoned.
inline bool is_seasoned(const Stave& b) {
    return b.sound && b.moisture <= AIR_DRY_EMC * SEASONED_BAND;
}

// Dry a billet for a stretch of (compressed) time, in years. Moisture relaxes toward the
// air-dry floor on a diffusion clock whose time constant grows with the SQUARE of the
// billet's thickness -- so a thin billet seasons in a season and the whole log never does.
// Nothing is added; water leaves. A waste billet (unsound) or no time changes nothing.
inline Stave season(const Stave& b, double years) {
    Stave out = b;
    if (!b.sound || years <= 0.0) return out;
    const double mm  = b.thickness * 1000.0;
    const double tau = SEASON_TAU_MM2 * mm * mm;              // years; grows with thickness^2
    const double mc  = AIR_DRY_EMC + (b.moisture - AIR_DRY_EMC) * std::exp(-years / tau);
    out.moisture = mc <= AIR_DRY_EMC + SEASON_SPECK ? AIR_DRY_EMC : mc;
    return out;
}

} // namespace wrought
