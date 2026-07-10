#pragma once

#include <cmath>
#include <algorithm>
#include "smelt.h"

// The forge. Phase A step 4: a spongy bloom becomes a solid bar, and the bar
// becomes a shaped tool. The forge has two halves and this file holds both:
// CONSOLIDATION (bloom -> bar), which densifies, and SHAPING (bar -> tool),
// which gives a form and hardens it. They are the same anvil and the same verb;
// what changes is what the hammer is for. Shaping also holds the chain's last
// finding, RED-SHORT: the hammer is the only tool that can find the sulfur three
// stages of separation could not remove -- and it finds it by cracking.
//
// This file also holds the copper fork. Copper does not consolidate -- it comes
// out of the furnace LIQUID and is CAST (see CASTING below), skipping the bloom
// and the connectivity floor entirely. But it REJOINS here: a cast billet is
// lifted onto the same anvil by the same stock(), drawn by the same draw(), and
// hardened by the same hardness() law as an iron bar. Iron and copper diverge at
// the furnace and meet again at the hammer.
//
// And it holds the alloy fork, BRONZE (see ALLOY below). Bronze is not a fourth
// metal reduced from a fourth ore -- it is copper and tin, each cast, then mixed
// liquid. It rejoins at the anvil exactly as copper does, worked by the same draw()
// and hardness(). What alloying changes is not the PATH but the two numbers the path
// reads: the tin dissolved in the copper lattice raises the hardness curve (the
// billet is harder before a single blow) and lowers the melting point (it pours
// cooler than copper). One foreign atom, both gifts -- the same solute doing both.
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
    double sulfur = 0.0;        // kg S dissolved in the metal (the red-short seed)
    double slag[N_ELEM] {};     // kg entrained slag, by element
    double drained[N_ELEM] {};  // kg slag shed in the furnace, by element
    double temperature = 288.0; // K
};

// The consolidated bar.
struct Bar {
    double iron = 0.0;          // kg metal remaining after scale loss
    double sulfur = 0.0;        // kg S still dissolved in the metal (survives the fire)
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
    b.sulfur = fired.bloom_sulfur; // dissolved in the metal, not in the drainable slag
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
    bar.sulfur = green.sulfur; // dissolved sulfur survives the fire -- scale is iron, not S
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
// CASTING: the copper path from furnace to stock, iron's fork not taken.
//
// A bloom is consolidated because it comes out SOLID -- a slag-filled sponge you
// must hammer dense, and the connectivity floor is the price. Copper comes out
// LIQUID (smelt.h, if the furnace cleared TM_COPPER), and a liquid does for free
// what the hammer never fully can: molten copper (~8 g/cm^3) and molten fayalitic
// slag (~3.7) separate by density, the slag floats, and the billet freezes
// essentially slag-free. So the copper billet skips consolidation entirely and its
// slag fraction is ~0, not ~1.5%. The connectivity floor was never about iron; it
// was about being solid at furnace heat. This is the downstream half of the
// furnace divergence -- and both halves are decided by the same one bit, molten.
struct Billet {
    double metal = 0.0;          // kg cast copper
    double slag[N_ELEM] {};      // kg entrained slag -- ~0 for a clean liquid pour
    double drained[N_ELEM] {};   // kg slag that floated off and was skimmed
    double tm = TM_COPPER;       // the metal's melting point (depressed if alloyed)
    double tin_fraction = 0.0;   // Sn / (Cu+Sn) by mass; 0 = pure copper, >0 = bronze
    double temperature = 288.0;  // K
    bool cast_clean = false;     // did it pour liquid (slag floated off) or freeze a sponge
};

// Cast a copper melt into a billet. If it poured molten, the slag floats off and
// the billet is clean; if the furnace never reached the melting point it would
// freeze as a slag-trapping sponge like a bloom -- so the same `molten` bit that
// makes copper CAST also makes it CLEAN.
inline Billet cast(const MeltResult& melt) {
    Billet b;
    b.metal = melt.metal_cu;
    b.tm = TM_COPPER;
    b.temperature = melt.temperature;
    b.cast_clean = melt.molten;
    for (int e = 0; e < N_ELEM; ++e) {
        if (melt.molten) {           // liquid: slag floats off, billet freezes clean
            b.slag[e]    = 0.0;
            b.drained[e] = melt.slag[e];
        } else {                     // never poured: a copper sponge, slag trapped
            b.slag[e]    = melt.slag[e];
            b.drained[e] = 0.0;
        }
    }
    return b;
}

// The billet's impurity in one number, the copper twin of slag_fraction(Bar).
inline double billet_slag_fraction(const Billet& b) {
    const double s = slag_mass(b.slag);
    const double m = b.metal + s;
    return m > 0.0 ? s / m : 0.0;
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

// Iron melts at 1811 K (1538 C); the melting points now live in smelt.h, where the
// furnace reads them to decide a metal's STATE. The recrystallization floor is a
// fraction of the SAME number -- one melting point, read by both the furnace (cast
// vs bloom) and the anvil (hot vs cold work). That is the axis copper rejoins on.

// Recrystallization onset as a fraction of the melting point: ~0.35-0.4 Tm for a
// cold-worked metal (the classic homologous-temperature rule). 0.4 puts iron's
// floor at ~724 K = ~451 C, between a dull-red heat and a black bar. AUTHORED,
// cited range. It sets WHERE the hot/cold line falls, not that there is one. And
// because it is a FRACTION OF THE MELTING POINT, it is metal-specific for free:
// copper's floor (0.4 * 1358 K = ~543 K) sits far below iron's, so a heat that is
// cold work for iron can be hot work for copper. Issue #23.
inline constexpr double RECRYST_FRACTION = 0.4;
inline constexpr double RECRYST_T = RECRYST_FRACTION * TM_IRON; // iron's floor

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

// ------------------------------------------------------------------------
// BRONZE: the alloy, and why a mixture is a new metal instead of a blend.
//
// Copper is soft: annealed, it sits at the hardness floor, and no smith ever put a
// working edge on pure copper. Tin is softer still -- it dents under a thumbnail. By
// every naive expectation a mix of the two should land BETWEEN them, softer than
// copper. It does the opposite. A tenth of tin dissolved into copper makes bronze,
// and bronze is harder than either parent AND melts below either -- it lands off the
// end of both scales, not in the middle. That is the finding, and it has one cause:
//
//   ALLOYING IS NOT AVERAGING. A tin atom is the wrong size for a copper lattice
//   site, and that single fact of foreignness does two things at once. In the SOLID
//   it strains the lattice around every solute atom, and those strain fields snag
//   the dislocations whose glide is what "soft" means -- so the alloy is harder, at
//   zero cold work, than the pure metal (solid-solution strengthening). In the
//   FREEZE it disorders the crystal the liquid is trying to build, lowering the
//   temperature at which order wins -- so the alloy melts cooler than the pure metal
//   (freezing-point depression). Harder solid and looser freeze are not two lucky
//   properties; they are ONE property -- the misfit of the solute -- read on two
//   axes. And both are gifts: the harder metal takes a real edge, and the cooler,
//   wider freeze pours into a mold far better than stiff pure copper ever did. That
//   pairing -- castable AND edge-holding -- is why an AGE is named after this alloy
//   and not after the copper that preceded it by millennia.
//
// Mechanically, alloying does not add a process. It moves the two numbers the
// existing hardness() and cast() already read: it raises the hardness curve (floor
// and ceiling together) and lowers the melting point. The alloy is then cast,
// stocked, drawn and hardened by the very same functions as pure copper -- the code
// demonstrates "one law" by changing only inputs, never the law.
//
// Per DESIGN.md's fidelity ceiling: the parent melting points are VERIFIED, but the
// DEPRESSION rate and the SOLID-SOLUTION hardening rate are AUTHORED envelopes
// (issue #26). Both are linear-in-tin stand-ins for curves that really bend (peak
// strength near the peritectic, a liquidus that is not a straight line). The finding
// rides their SIGNS -- tin lowers the melt and raises the hardness, monotonically --
// never their magnitudes.

// Melting-point depression per unit tin fraction. bronze_tm slides from copper's Tm
// toward tin's as tin rises; at x=0.10 this puts the (nominal) melt near 1273 K
// (~1000 C), below copper's 1085 C, matching bronze's easier casting. AUTHORED,
// cited envelope (issue #26). Sign, not size, carries the finding.
inline constexpr double MELT_DEPRESSION = 850.0; // K per unit Sn fraction

// Solid-solution hardening per unit tin fraction, in Brinell, applied to BOTH the
// annealed floor and the saturated ceiling (the whole curve lifts). At x=0.10 it
// adds ~40 HB: a cast bronze already reads above pure copper's annealed floor, and
// cold-worked bronze tops even cold-worked iron -- which is the historical reason
// early wrought iron did not displace bronze until it could be made into steel.
// AUTHORED, cited envelope (issue #26).
inline constexpr double SOLUTE_HARDENING = 400.0; // HB per unit Sn fraction

inline double clamp_tin(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

// Bronze's melting point at tin fraction x: depressed below copper's. x=0 returns
// TM_COPPER exactly, so a pure-copper billet is unchanged.
inline double bronze_tm(double x) { return TM_COPPER - MELT_DEPRESSION * clamp_tin(x); }

// Bronze's annealed and saturated hardness at tin fraction x: the pure-copper
// values (H_ANNEALED, H_SATURATED) plus the solute lift. x=0 returns the pure values
// exactly, so pure copper and iron keep the unshifted curve and their tests do not move.
inline double bronze_h_annealed(double x)  { return H_ANNEALED  + SOLUTE_HARDENING * clamp_tin(x); }
inline double bronze_h_saturated(double x) { return H_SATURATED + SOLUTE_HARDENING * clamp_tin(x); }

// Mix a copper pour and a tin pour into a bronze billet. This is the alloying step:
// two liquids become one, the tin fraction is set by mass, and from it the billet's
// melting point and hardness curve are fixed. Both metals were cast clean (each
// poured liquid and shed its slag), so bronze inherits copper's cleanliness -- the
// alloy is clean for the same reason its parents were. The ledger is trivial: metal
// out = Cu in + Sn in, no element created or lost.
inline Billet alloy(const MeltResult& copper, const MeltResult& tin) {
    Billet b;
    const double cu = copper.metal_cu, sn = tin.metal_sn;
    const double total = cu + sn;
    b.metal = total;
    b.tin_fraction = (total > 0.0) ? sn / total : 0.0;
    b.tm = bronze_tm(b.tin_fraction);
    b.cast_clean = copper.molten && tin.molten;   // both poured liquid -> clean alloy
    b.temperature = std::fmax(copper.temperature, tin.temperature);
    // Both parents cast clean, so the billet carries no entrained slag; any slag the
    // two pours skimmed is recorded as drained, conserved but out of the metal.
    for (int e = 0; e < N_ELEM; ++e) {
        b.slag[e]    = 0.0;
        b.drained[e] = copper.slag[e] + tin.slag[e];
    }
    return b;
}

// ------------------------------------------------------------------------
// RED-SHORT: the poison that only the hammer can find.
//
// Shaping said the heat is a smith's friend: form is free hot, so you work hot
// to move metal and save the cold, hardening blows for the end. RED-SHORT is the
// exact inversion of that friendship, and it is triggered by the one thing three
// stages of separation could not remove -- the sulfur that dissolved into the
// iron back in the bloomery (smelt.h) and rode the metal, invisible, through
// consolidation to this anvil. The finding:
//
//   SULFUR MAKES HOT THE FORBIDDEN END. Iron and iron-sulfide form a eutectic
//   that melts at ~988 C -- far below a forging heat. So when a sulfur-bearing
//   bar is worked HOT, a liquid film wets its grain boundaries, and the metal
//   tears itself apart along them under the hammer: it crumbles, it will not
//   weld, it cracks. Cold, the film is solid and the bar holds -- but cold is
//   where metal will not flow and where it work-hardens toward brittleness. So
//   dirty iron is trapped: it red-shorts at the hot end where clean iron flows
//   free, and it barely moves at the cold end where it is merely sound. The
//   contaminant no pan, no sluice, no lodestone could see is the one that decides
//   whether the metal can be forged at all -- and it is revealed only here, at
//   the last blow, under deformation. This is why pyritic ore was worthless
//   despite being iron-rich, and it closes the iron chain: the whole point of the
//   lodestone and the crush was never just yield, it was to keep this poison out.
//
// Per DESIGN.md's fidelity ceiling -- look up the reaction, derive the balance --
// the eutectic temperature is VERIFIED (a tabulated phase equilibrium, not a
// force balance). What is AUTHORED is the sulfur THRESHOLD at which a continuous
// embrittling film forms and the RATE at which hot strain then opens cracks
// (issue #24). The finding depends on the eutectic existing below forging heat
// and on cracking being monotone in hot sulfur-strain, not on those values.

// Fe-FeS eutectic melting point: 988 C = 1261 K. Above it, a sulfur-bearing
// bar's grain boundaries carry a liquid film. VERIFIED (Fe-S phase diagram);
// well below any forging or welding heat, which is the whole trap.
inline constexpr double EUTECTIC_T = 1261.0;

// Sulfur mass fraction of the metal above which the grain-boundary sulfide forms
// a continuous embrittling film. Hot shortness sets in above ~0.05 wt% S when
// there is no manganese to fix it, as bloomery iron has none. AUTHORED, cited
// threshold: it sets WHERE red-short begins, not that it does. Issue #24.
inline constexpr double RED_SHORT_S = 5.0e-4;

// Cracking e-folding: how much (excess-sulfur x hot strain) opens grain-boundary
// cracks. Soundness decays as exp(-cracking), so this only sets how fast a
// red-short bar comes apart. AUTHORED, finding-independent in size. Issue #24.
inline constexpr double CRACK_SCALE = 0.02;

// A shaped piece. Pure deformation: it carries the bar's metal and stringers
// unchanged, plus the two things shaping creates -- a form (elongation, length
// as a multiple of the stock bar) and a history of cold strain that survived.
struct Tool {
    double metal = 0.0;         // kg metal, unchanged by pure deformation (Fe or Cu)
    double sulfur = 0.0;        // kg S dissolved in the metal (rode in from the bloom)
    double slag[N_ELEM] {};     // the bar's stringers, drawn out along the grain
    double elongation = 1.0;    // length as a multiple of the stock bar
    double cold_strain = 0.0;   // accumulated plastic strain worked in below recryst
    double cracking = 0.0;      // grain-boundary damage from hot-working a sulfurous bar
    double tm = TM_IRON;        // the metal's melting point; the recryst floor is a fraction of it
    double h_annealed = H_ANNEALED;   // this metal's soft floor (raised by alloying)
    double h_saturated = H_SATURATED; // this metal's hard ceiling (raised by alloying)
    double temperature = 288.0; // K, the piece's current heat
};

// Lift a consolidated iron bar onto the anvil. Nothing deforms yet.
inline Tool stock(const Bar& bar) {
    Tool t;
    t.metal = bar.iron;
    t.sulfur = bar.sulfur;
    t.tm = TM_IRON;
    t.temperature = bar.temperature;
    for (int e = 0; e < N_ELEM; ++e) t.slag[e] = bar.slag[e];
    return t;
}

// Lift a cast copper billet onto the SAME anvil. This is the rejoin: a copper
// billet and an iron bar become the same kind of Tool, worked by the same draw()
// and hardened by the same hardness() law -- they differ only in the melting point
// they carry, which sets each metal's recryst floor. Dislocations do not care which
// metal tangled them; the code says so by not caring which struct fed the stock.
inline Tool stock(const Billet& billet) {
    Tool t;
    t.metal = billet.metal;
    t.sulfur = 0.0;             // oxide-ore copper/bronze carries no sulfur: no red-short
    t.tm = billet.tm;           // depressed if this is bronze (alloy())
    // The hardness curve the piece carries onto the anvil is set here, from the
    // billet's tin fraction: pure copper (x=0) keeps the unshifted floor and ceiling,
    // bronze enters already lifted. This is where "the alloy is harder before a blow"
    // becomes a fact the same hardness() law will read.
    t.h_annealed  = bronze_h_annealed(billet.tin_fraction);
    t.h_saturated = bronze_h_saturated(billet.tin_fraction);
    t.temperature = billet.temperature;
    for (int e = 0; e < N_ELEM; ++e) t.slag[e] = billet.slag[e];
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
    // Hardness: only strain below THIS metal's recrystallization floor survives.
    // The floor is a fraction of the metal's own melting point, so copper (low Tm)
    // anneals at heats where iron (high Tm) is still cold-working -- same law, same
    // draw, different floor. That is where the two metals' paths rejoin.
    const double recryst = RECRYST_FRACTION * t.tm;
    if (T < recryst) t.cold_strain += eps;

    // Red-short: above the Fe-FeS eutectic a sulfurous bar's grain boundaries are
    // wetted by a liquid film, and hot strain tears the metal along them. Below
    // the eutectic the film is solid and the bar holds, however dirty. So the
    // crack is a HOT failure, and only for metal above the sulfur threshold. Clean
    // metal -- copper from oxide ore carries no sulfur -- never triggers it.
    const double s_frac = (t.metal + t.sulfur > 0.0) ? t.sulfur / (t.metal + t.sulfur) : 0.0;
    if (T >= EUTECTIC_T && s_frac > RED_SHORT_S)
        t.cracking += (s_frac - RED_SHORT_S) / CRACK_SCALE * eps;

    t.temperature = T;
}

// Brinell hardness of the piece: from the annealed floor toward a saturated
// ceiling, exponential in the retained cold strain. Worked hot only, it stays
// soft; worked cold, it climbs and levels off. DERIVED form, AUTHORED scale.
inline double hardness(const Tool& t) {
    return t.h_annealed + (t.h_saturated - t.h_annealed) *
           (1.0 - std::exp(-t.cold_strain / STRAIN_SCALE));
}

// Structural soundness of the piece: 1 is whole, 0 is come apart. Grain-boundary
// cracking from hot-working a sulfurous bar decays it exponentially. A clean bar,
// or a dirty one kept below the eutectic, stays exactly 1 -- it is only hot work
// on sulfurous metal that spends soundness. This is the red-short verdict.
inline double soundness(const Tool& t) { return std::exp(-t.cracking); }

} // namespace wrought
