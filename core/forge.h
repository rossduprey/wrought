#pragma once

#include <cmath>
#include <algorithm>
#include "smelt.h"

// The forge. Phase A step 4: a spongy bloom becomes a solid bar, and the bar
// becomes a shaped tool. The forge has two halves and this file holds both:
// CONSOLIDATION (bloom -> bar), which densifies, and SHAPING (bar -> tool),
// which gives a form and hardens it. They are the same anvil and the same verb;
// what changes is what the hammer is for.
//
// This is the first process in the project that is DEFORMATION, not separation.
// Everything before it -- pan, sluice, lodestone, even the bloomery's chemistry --
// sorts a population of grains by a property. Consolidation does something new: it
// squeezes a single porous solid and drives out what is not iron. The verb is the
// hammer, and the state that evolves is the bloom's porosity.
//
// A bloom does not come out of the furnace as metal. It comes out as a SPONGE:
// grains of reduced iron welded into a lattice at maybe half density, its pores
// filled with molten fayalite slag (the same slag the fayalite wall put there,
// smelt.h). Hammering at welding heat does two coupled things -- it closes the
// pores (densifies), and it squeezes the slag they held out of the piece. But the
// second one has a floor, and the floor is the whole finding:
//
//   SLAG DRAINS ONLY THROUGH CONNECTED PORES. As the sponge densifies, its
//   porosity falls, and below a percolation threshold the remaining pores stop
//   touching each other. The slag trapped in those isolated pores has no path to
//   the surface, and no amount of hammering removes it. So a wrought bar always
//   carries slag -- as stringers, threaded through the metal -- and the residual
//   is set by CONNECTIVITY, not by effort. Hammer twice as long and you get the
//   same 1.5%.
//
// That floor is the deformation-era twin of the fayalite wall: a thing the process
// cannot get under, forced by geometry rather than authored. It is why wrought iron
// is wrought -- the stringers are its signature, and they are why it tears along
// the grain and welds to itself so willingly.
//
// And consolidation pays the project's central law one more time, on a new axis.
// Every heat that expels slag also oxidizes the bar's surface: iron flakes off as
// scale and leaves the piece. So driving the slag toward its floor costs metal --
// a cleaner bar is a smaller bar. Grade against recovery, the pan's law, now in the
// fire. We have implemented it once and found it four times: on settling velocity,
// on grain size, across cleaning stages, and here under the hammer.
//
// Per DESIGN.md's fidelity ceiling -- derive the force balance, tabulate the
// reaction -- the two floors here are DERIVED (the connectivity threshold is
// geometry; the slag a porosity holds is volume arithmetic against two densities).
// What is AUTHORED is the RATE: how fast a heat drains connected slag, and how much
// iron a heat scales away. Those are the forge's version of the pan's sharpness --
// physical process, authored speed -- and they are a tracked issue (#22). The
// findings below do not depend on their values, only on their signs.

namespace wrought {

// Fayalite slag density, g/cm^3. Fe2SiO4 measured 4.39 (solid); molten is nearer
// 3.7, and a welding-heat bloom is pasty between the two. VERIFIED (solid fayalite,
// Deer/Howie/Zussman). The floor's VALUE scales with this over iron's density; the
// finding that a floor EXISTS does not touch it.
inline constexpr double RHO_SLAG = 4.39;

// Solid fraction of the green sponge as it leaves the furnace: ~0.55, so porosity
// ~0.45, all of it slag-filled. AUTHORED, bounded (a fresh bloom is 0.4-0.6 dense).
// It sets how much slag the bloom starts with -- and the floor result is asserted
// insensitive to it. Issue #22.
inline constexpr double GREEN_PACKING = 0.55;

// Porosity at which pores stop connecting and slag can no longer drain. 3D
// continuum percolation lives around 0.03-0.16 depending on pore geometry; 0.03 is
// the low end (well-connected overlapping pores pinch off late) and lands the
// residual in the measured 1-4% wrought-iron range. AUTHORED, cited range. It sets
// where the floor sits, not that there is one. Issue #22.
inline constexpr double CONNECTIVITY_FLOOR = 0.03;

// Fraction of the drainable (above-floor) slag a single heat expels. AUTHORED. Any
// value in (0,1) gives the same floor and the same monotone tradeoff; only the
// number of heats to get there moves. Issue #22.
inline constexpr double SLAG_EXPULSION = 0.40;

// Fraction of the bar's iron lost to scale per heat -- oxidized at the surface and
// flaked off on the anvil. Historically a few percent per heat. AUTHORED. Its sign,
// not its size, is what makes purity cost yield. Issue #22.
inline constexpr double SCALE_PER_HEAT = 0.025;

// Slag mass a porosity phi holds around a unit iron mass: the pores are volume
// fraction phi, the iron is 1-phi, so slag_vol/iron_vol = phi/(1-phi), times the
// density ratio. DERIVED -- geometry and two densities, no author.
inline double slag_per_iron(double phi) {
    return (phi / (1.0 - phi)) * (RHO_SLAG / PHASES[IRON].density);
}

// The green bloom: metallic iron plus the slag its pores hold, element-resolved.
// `drained` is slag the furnace already shed (tapped, or run off the bottom) before
// the hammer touched it -- everything the pores could not hold.
struct Bloom {
    double iron = 0.0;          // kg metallic Fe (the sponge's solid)
    double slag[N_ELEM] {};     // kg entrained slag, by element
    double drained[N_ELEM] {};  // kg slag shed in the furnace, by element
    double temperature = 288.0; // K
};

// The consolidated bar.
struct Bar {
    double iron = 0.0;          // kg metal remaining after scale loss
    double slag[N_ELEM] {};     // kg residual slag, the stringers
    double scale[N_ELEM] {};    // kg iron lost to hammer scale (left the piece)
    double drained[N_ELEM] {};  // kg slag squeezed out (furnace + every heat)
    int heats = 0;
    double temperature = 288.0;
};

inline double slag_mass(const double s[N_ELEM]) {
    double m = 0.0;
    for (int e = 0; e < N_ELEM; ++e) m += s[e];
    return m;
}

// Pull a bloom from a fired bloomery. The smelt reports how much iron reduced and
// how much slag formed; how that slag divides between the bloom's pores and the
// furnace floor is a MECHANICAL question, and it is answered here, not in smelt.h.
// The pores fill first (they are where the slag freezes around the sponge); any
// slag beyond what they hold has drained.
inline Bloom pull_bloom(const BloomResult& fired, double packing = GREEN_PACKING) {
    Bloom b;
    b.iron = fired.bloom_iron;
    b.temperature = fired.bloom.temperature;

    const double total = slag_mass(fired.slag);
    const double capacity = fired.bloom_iron * slag_per_iron(1.0 - packing);
    const double entrained = std::fmin(total, capacity);
    const double frac = (total > 0.0) ? entrained / total : 0.0;

    for (int e = 0; e < N_ELEM; ++e) {
        b.slag[e]    = frac * fired.slag[e];
        b.drained[e] = (1.0 - frac) * fired.slag[e];
    }
    return b;
}

// Hammer the bloom for `heats` cycles. Each heat drains a fraction of the connected
// (above-floor) slag toward the surface, and scales a fraction of the iron off it.
// The floor -- the slag isolated pores hold at the connectivity threshold -- never
// leaves. The ledger balances by construction: green iron + slag = bar iron + bar
// slag + scale + drained.
inline Bar consolidate(const Bloom& green, int heats,
                       double expulsion = SLAG_EXPULSION,
                       double scale_rate = SCALE_PER_HEAT) {
    Bar bar;
    bar.iron = green.iron;
    bar.temperature = green.temperature;
    bar.heats = heats;
    for (int e = 0; e < N_ELEM; ++e) {
        bar.slag[e]    = green.slag[e];
        bar.drained[e] = green.drained[e];
    }

    for (int n = 0; n < heats; ++n) {
        const double total = slag_mass(bar.slag);
        const double floor = bar.iron * slag_per_iron(CONNECTIVITY_FLOOR);
        const double connected = std::fmax(0.0, total - floor);
        const double expelled = connected * expulsion;
        const double f = (total > 0.0) ? expelled / total : 0.0;
        for (int e = 0; e < N_ELEM; ++e) {
            const double moved = f * bar.slag[e];
            bar.slag[e]    -= moved;
            bar.drained[e] += moved;
        }
        const double lost = bar.iron * scale_rate;
        bar.iron       -= lost;
        bar.scale[EL_FE] += lost;
    }
    return bar;
}

// The bar's impurity in one number: slag as a fraction of the bar's own mass.
inline double slag_fraction(const Bar& bar) {
    const double s = slag_mass(bar.slag);
    const double m = bar.iron + s;
    return m > 0.0 ? s / m : 0.0;
}

// Metal kept, as a fraction of the iron that entered as a bloom.
inline double yield(const Bloom& green, const Bar& bar) {
    return green.iron > 0.0 ? bar.iron / green.iron : 0.0;
}

// ------------------------------------------------------------------------
// SHAPING: bar -> tool. The forge's second half.
//
// Consolidation was deformation that only densifies -- it drives out slag and
// closes pores but leaves a shapeless lump. Shaping is deformation that gives a
// FORM (a bar drawn to a rod, a rod to a blade) and, if done cold, WORK-HARDENS
// it. The single control that couples the two is TEMPERATURE, and that coupling
// is the whole finding of this half:
//
//   FORM IS FREE AT ANY HEAT; HARDNESS IS ONLY COLD. Plastic strain always
//   gives form -- squeeze the section and the piece gets longer, hot or cold,
//   because volume is conserved. But hardness comes from dislocations piling up
//   in the crystal, and above the recrystallization temperature (~0.4 of iron's
//   melting point, ~450 C) the metal re-grows fresh grains as fast as the hammer
//   tangles them, so hot work leaves NO hardness behind. Only strain put in
//   below that floor survives. So the smith faces a bind that is not a matter of
//   skill: to move a lot of metal you work it hot, where it flows -- but a tool
//   needs a hard edge, and hardness can only be beaten in cold, in small bites,
//   near the end. Form and hardness are won at opposite ends of the heat.
//
// The recrystallization floor is the shaping-side analogue of the fayalite wall
// and the connectivity floor: a real temperature (a fraction of a melting point,
// both VERIFIED), not an authored knob. What is AUTHORED is the hardening RATE
// and its ceiling -- how fast dislocations accumulate and how hard the metal can
// get -- exactly as the drain and scale rates were for consolidation. The
// findings depend on the recryst floor's existence and on hardening being
// monotone and saturating, not on the numbers' values (issue #23).
//
// Shaping is PURE deformation: it conserves the metal exactly and removes no
// slag -- the stringers do not leave, they get drawn out ALONG the piece, which
// is why a wrought bar tears along its length and why the grain follows the
// form. (Surface scale during shaping reheats is the same oxidation already
// modelled per-heat in consolidate(); it is not re-charged here, so a draw moves
// no mass.) This is the last stop of the iron chain; it is also the axis copper
// SHARES with iron, so it is where the two metals' paths rejoin downstream.

// Iron melts at 1811 K (1538 C). VERIFIED, standard. The recrystallization floor
// is a fraction of this; the floor's existence, not this value, is load-bearing.
inline constexpr double TM_IRON = 1811.0;

// Recrystallization onset as a fraction of the melting point: ~0.35-0.4 Tm for a
// cold-worked metal (the classic homologous-temperature rule). 0.4 puts it at
// ~724 K = ~451 C, between a dull-red heat and a black bar. AUTHORED, cited
// range. It sets WHERE the hot/cold line falls, not that there is one. Issue #23.
inline constexpr double RECRYST_FRACTION = 0.4;
inline constexpr double RECRYST_T = RECRYST_FRACTION * TM_IRON;

// Hardness scale (Brinell): annealed soft iron to heavily cold-worked. Roughly
// right for ferrite, but AUTHORED and finding-independent -- the result is that
// cold work is HARDER than hot and that it SATURATES, which needs only
// H_SATURATED > H_ANNEALED, not either value. Issue #23.
inline constexpr double H_ANNEALED  = 90.0;
inline constexpr double H_SATURATED = 220.0;

// Retained-cold-strain e-folding of the hardening curve: dislocation density
// saturates, so hardness approaches its ceiling exponentially in accumulated
// cold strain. AUTHORED. Its sign and finiteness give monotone-and-saturating;
// its size only sets how fast. Issue #23.
inline constexpr double STRAIN_SCALE = 0.6;

// A shaped piece. Pure deformation: it carries the bar's metal and stringers
// unchanged, plus the two things shaping creates -- a form (elongation, length
// as a multiple of the stock bar) and a history of cold strain that survived.
struct Tool {
    double iron = 0.0;          // kg metal, unchanged by pure deformation
    double slag[N_ELEM] {};     // the bar's stringers, drawn out along the grain
    double elongation = 1.0;    // length as a multiple of the stock bar
    double cold_strain = 0.0;   // accumulated plastic strain worked in below recryst
    double temperature = 288.0; // K, the piece's current heat
};

// Lift a consolidated bar onto the anvil. Nothing deforms yet.
inline Tool stock(const Bar& bar) {
    Tool t;
    t.iron = bar.iron;
    t.temperature = bar.temperature;
    for (int e = 0; e < N_ELEM; ++e) t.slag[e] = bar.slag[e];
    return t;
}

// One hammer pass at temperature `T`: reduce the cross-section by fraction
// `reduction` (0..1). Volume conservation turns that into elongation for free at
// any heat; only strain put in below the recrystallization floor accumulates as
// the cold strain that hardens. Metal and slag are untouched -- pure deformation.
inline void draw(Tool& t, double reduction, double T) {
    if (reduction <= 0.0) { t.temperature = T; return; }
    const double r = std::fmin(reduction, 1.0 - 1e-9);
    const double eps = -std::log(1.0 - r);   // true strain of this pass
    t.elongation *= std::exp(eps);           // form: won hot or cold
    if (T < RECRYST_T) t.cold_strain += eps; // hardness: only cold survives
    t.temperature = T;
}

// Brinell hardness of the piece: from the annealed floor toward a saturated
// ceiling, exponential in the retained cold strain. Worked hot only, it stays
// soft; worked cold, it climbs and levels off. DERIVED form, AUTHORED scale.
inline double hardness(const Tool& t) {
    return H_ANNEALED + (H_SATURATED - H_ANNEALED) *
           (1.0 - std::exp(-t.cold_strain / STRAIN_SCALE));
}

} // namespace wrought
