#pragma once

#include <cmath>
#include "levigate.h"

// Clay into pot. This is the file where the ratchet is supposed to live, and
// therefore the file to attack first: it holds every authored bridge in Era 1,
// and there are two.
//
// DESIGN.md states the ratchet like this:
//
//   > How finely you levigate your clay sets how densely your pan fires. How
//   > dense your pan is sets its grade/recovery curve. That curve sets the purity
//   > of your next concentrate -- including your next batch of clay.
//
// The first clause is true and is ceramics. The **second clause is the one to
// doubt**, and this file exists to make it falsifiable rather than repeat it.

namespace wrought {

// ---------------------------------------------------------------------------
// What a fired body is made of.

inline double clay_grade(const Substance& body) { return body.grade(KAOLINITE); }

// The size of the coarsest thing embedded in the fired surface, mass-weighted
// over everything that is not a clay mineral. Kaolinite vitrifies; the rest sits
// in the fabric as grit and stands proud of it. Derived, not authored.
inline double grit_diameter(const Substance& body) {
    double m = 0.0, md = 0.0;
    for (int p = 0; p < N_PHASE; ++p) {
        if (p == KAOLINITE) continue;
        for (int s = 0; s < N_SIZE; ++s) {
            const double cell = body.freegrain[p][s] + body.composite[p][s];
            m += cell;
            md += cell * bin_diameter(s);
        }
    }
    return m > 0.0 ? md / m : 0.0;
}

// ---------------------------------------------------------------------------
// BRIDGE 1 — grit into sharpness. The claim DESIGN.md makes.
//
// A pan's cut is the shear velocity of the water over its floor. A rough floor
// makes that velocity vary from place to place: a grain of grit standing proud
// by `d` into a skin of thickness `h` sits where the water moves measurably
// faster than at the floor. So the pan does not have one cut, it has a
// distribution of them, and the partition the pan *reports* is the average of
// many logistics with jittered centres -- which is a wider logistic. Variances
// add:
//
//     sigma_pan^2 = sigma_wrist^2 + (d_grit / h_skin)^2
//
// The quadrature is derivable (a logistic is a probit in disguise, and mixing a
// probit over a Gaussian jitter in its centre gives a probit of the summed
// variance). The identification of the ln-velocity spread with `d_grit/h_skin`
// is **AUTHORED**: it says the velocity grows roughly linearly across the skin,
// which is true of a viscous sublayer and only roughly true here.
//
// `h_skin` is not new -- it is the same skin the bed already needed, so this
// bridge introduces no constant of its own.

inline double WRIST_SHARPNESS = 0.55;  // AUTHORED. The blur of a human wrist,
                                       // and the irreducible floor of any pan.
                                       // It is PAN.sharpness, renamed for what
                                       // it is: a property of the operator, not
                                       // of the pot.

inline double skin_depth() { return SKIN_GRAINS * bin_diameter(SAND); }

inline SeparatorParams fire_pan(const Substance& body) {
    const double s = grit_diameter(body) / skin_depth();
    return SeparatorParams{PAN.cut_velocity, std::hypot(WRIST_SHARPNESS, s), "fired pan"};
}

// ---------------------------------------------------------------------------
// BRIDGE 2 — clay into vessel. The claim DESIGN.md does not make, and which
// turns out to be the one that carries the loop.
//
// A green body stands up because clay platelets are plastic and grit is not.
// Below some clay fraction a pinch pot will not hold its own wall; at pure clay
// it stands to some height and no higher, because a wet wall creeps. So the
// tallest wall you can throw scales with the clay fraction of your body, and the
// pot's volume with the cube of that -- a pot being a shape, not a wall.
//
// MAX_WALL is AUTHORED and UNVERIFIED. The linearity is authored too, and it is
// authored because we have no data, not because we have a reason. Both should be
// a tracked issue. Everything downstream of them is geometry.
inline constexpr double MAX_WALL = 0.30;  // m, a pure-clay pinch pot. UNVERIFIED.

inline Vessel throw_pot(const Substance& body) {
    const double h = MAX_WALL * clay_grade(body);
    return Vessel{h, 0.5 * h, "a fired pot"};   // a bowl twice as wide as it is deep
}

} // namespace wrought
