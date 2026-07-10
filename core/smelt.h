#pragma once

#include <cmath>
#include "substance.h"

// The bloomery. Phase A step 3: slag falls out, and the ledger balances.
//
// This is the first process in the project that is CHEMISTRY, not mechanics, and
// DESIGN.md's fidelity ceiling says exactly where to stop: "derive where the
// derivation is a force balance, look up where it is a reaction." Reduction is a
// reaction, so its temperature and atmosphere are a tabulated envelope, not a
// Gibbs solve. But one number here is not looked up -- it is forced by two atomic
// weights, and it is the whole finding:
//
//   A BLOOMERY HAS NO FLUX. The only way for silica to leave the furnace is to
//   combine with iron as fayalite, Fe2SiO4 = 2 FeO + SiO2. Every kilogram of
//   silica in the charge therefore drags 2 moles of iron per mole of silica into
//   the slag and out of the bloom:
//
//     iron lost to slag per kg silica = 2 * M(Fe) / M(SiO2) = 1.859 kg.
//
//   So a charge yields metal at all only if its iron-to-silica MASS ratio clears
//   1.859. Below that, the iron is eaten by its own gangue and you get a glassy
//   slag and no bloom. This is why nobody in history smelted panned black sand
//   (Fe/SiO2 ~ 0.1, and no pan can lift it past ~0.5) and why bog iron and
//   magnetically-cleaned ironsand were what humans actually used. It is also why
//   the lodestone (magnetic.h) is not a luxury but the gate: it is the only Era-1
//   tool that pushes Fe/SiO2 across 1.859.
//
// The wall is derived, not authored. What is authored is the reaction envelope
// (temperature, CO-rich atmosphere) and the simplifying assumption that ALL
// silica -- free quartz and silicate-bound alike -- reports to a pure fayalite
// slag. Real slags carry alumina, lime and free silica and melt at other
// temperatures; those move the wall a little, and they are a tracked issue (#21).
//
// One element on the ledger is not here for the balance -- it is here because it
// RIDES. Sulfur, carried by pyrite, is not rejected by any Era-1 tool: it is not
// magnetic, it does not settle apart, and the bloomery's acidic slag, having no
// lime flux, is a poor sulfur sink -- so a fraction of it dissolves into the
// reduced iron and stays there, invisible, all the way to the anvil. It does no
// harm in this file. It does its harm two files downstream, at the last blow of
// the hammer, where it makes the metal RED-SHORT (forge.h). Tracking it here is
// what lets that failure exist at all: it is the contaminant no separation can
// see, revealed only under deformation. Issue #24.

namespace wrought {

// Elements we account. S is tracked on its own (not lumped) because it is the one
// impurity that survives into the metal and breaks it downstream -- red-short, in
// forge.h. Cu is tracked because it is the second METAL this chain wins: the same
// ledger that balances iron's smelt balances copper's, which is the point -- one
// bookkeeping, two metals. OTHER still lumps Ti, Ca, Al, K, H, Mn... -- everything
// with nowhere to go but slag. All are conserved; S and Cu have fates outside it
// (S into iron, Cu becoming the metal).
enum Element { EL_FE, EL_SI, EL_O, EL_C, EL_S, EL_CU, EL_OTHER, N_ELEM };

// A mineral's formula, as counts of the tracked atoms per formula unit plus the
// molar mass. Everything else in the formula falls into OTHER = M - tracked.
// Atomic weights are IUPAC 2021 standard values -- the most citable numbers in
// the project, and the reason nothing in this file is UNVERIFIED.
struct Formula { double molar_mass, n_fe, n_si, n_o, n_c, n_s, n_cu; };

namespace detail {
    constexpr double A_FE = 55.845, A_SI = 28.085, A_O = 15.999, A_C = 12.011,
                     A_S = 32.06, A_CU = 63.546;
    // Per-phase formulae, indexed by PhaseId. A zero row means "no tracked atoms",
    // which is correct for a bloom of metallic iron carrying only Fe. Pyrite is the
    // only sulfur-bearer: FeS2 is now fully resolved (Fe + 2 S ~= its whole mass),
    // so its OTHER falls to ~0 and its sulfur is on the ledger where it can do harm.
    // Cuprite (Cu2O) and metallic copper carry the copper column; every other row
    // is copper-free.
    inline constexpr Formula FORMULA[N_PHASE] = {
        /*QUARTZ    SiO2         */ {60.083,  0, 1, 2, 0, 0, 0},
        /*FELDSPAR  KAlSi3O8     */ {278.33,  0, 3, 8, 0, 0, 0},
        /*KAOLINITE Al2Si2O5(OH)4*/ {258.16,  0, 2, 9, 0, 0, 0},
        /*MAGNETITE Fe3O4        */ {231.53,  3, 0, 4, 0, 0, 0},
        /*HEMATITE  Fe2O3        */ {159.69,  2, 0, 3, 0, 0, 0},
        /*GOETHITE  FeO(OH)      */ {88.851,  1, 0, 2, 0, 0, 0},
        /*ILMENITE  FeTiO3       */ {151.71,  1, 0, 3, 0, 0, 0},
        /*PYRITE    FeS2         */ {119.97,  1, 0, 0, 0, 2, 0},
        /*CALCITE   CaCO3        */ {100.09,  0, 0, 3, 1, 0, 0},
        /*CARBON    C            */ {12.011,  0, 0, 0, 1, 0, 0},
        /*IRON      Fe (metal)   */ {55.845,  1, 0, 0, 0, 0, 0},
        /*CUPRITE   Cu2O         */ {143.09,  0, 0, 1, 0, 0, 2},
        /*COPPER    Cu (metal)   */ {63.546,  0, 0, 0, 0, 0, 1},
    };
}

// Mass fraction of element e in phase p.
inline double element_fraction(int p, int e) {
    const Formula& f = detail::FORMULA[p];
    const double tracked = f.n_fe * detail::A_FE + f.n_si * detail::A_SI
                         + f.n_o * detail::A_O + f.n_c * detail::A_C
                         + f.n_s * detail::A_S + f.n_cu * detail::A_CU;
    switch (e) {
        case EL_FE:    return f.n_fe * detail::A_FE / f.molar_mass;
        case EL_SI:    return f.n_si * detail::A_SI / f.molar_mass;
        case EL_O:     return f.n_o  * detail::A_O  / f.molar_mass;
        case EL_C:     return f.n_c  * detail::A_C  / f.molar_mass;
        case EL_S:     return f.n_s  * detail::A_S  / f.molar_mass;
        case EL_CU:    return f.n_cu * detail::A_CU / f.molar_mass;
        case EL_OTHER: return (f.molar_mass - tracked) / f.molar_mass;
    }
    return 0.0;
}

// Element masses (kg) in a substance, summed over every phase and size. Composite
// grains are handled by phase_mass, which already splits a composite's mass into
// its target phase and its gangue -- so a locked magnetite grain's quartz shows
// up in the silica total exactly as it should.
inline void assay_elements(const Substance& s, double out[N_ELEM]) {
    for (int e = 0; e < N_ELEM; ++e) {
        out[e] = 0.0;
        for (int p = 0; p < N_PHASE; ++p) out[e] += s.phase_mass(p) * element_fraction(p, e);
    }
}

// Iron the bloomery can reduce: the oxides. Silicate-locked iron is not here (it
// stays in slag) -- but the phase table has none. Ilmenite's iron is refractory
// and reports to slag; pyrite is a sulfide -- its iron does not reduce here, and
// its SULFUR is the poison (partitioned below, not to slag alone). So the
// reducible set is exactly the three iron oxides. Issue #21.
inline bool reducible(int p) { return p == MAGNETITE || p == HEMATITE || p == GOETHITE; }

// Fraction of the charge's sulfur that dissolves into the reduced iron rather than
// leaving with the slag. A bloomery's acidic fayalitic slag, with no lime flux, is
// a poor sulfur sink, so a large share stays in the metal -- which is exactly why
// bloomery iron from pyritic ore was red-short and worthless. AUTHORED, finding-
// independent: the sign (some sulfur reaches the metal), not the size, is what lets
// red-short exist. Manganese, which would fix the sulfur harmlessly, is an Era-3
// answer this chain does not have yet. Issue #24.
inline constexpr double SULFUR_TO_METAL = 0.5;

// M(SiO2)/M(Si): mass of silica per unit mass of silicon.
inline constexpr double SIO2_PER_SI = 60.083 / 28.085;
// Iron dragged into slag per unit mass of silica, as fayalite Fe2SiO4.
inline constexpr double FE_PER_SIO2 = 2.0 * 55.845 / 60.083; // = 1.859

// Stoichiometric carbon to reduce iron oxide to metal: ~0.32 kg C per kg Fe. From
// hematite, 3 O are stripped per 2 Fe (1.5 O/Fe), and one carbon carries off one
// oxygen, so 1.5 mol C per mol Fe. DERIVED -- it is atomic weights. Real
// bloomeries burn several times this for heat and blast, which is a fuel question,
// not a stoichiometry one, and lives outside this closed charge ledger.
inline constexpr double CARBON_PER_FE = 1.5 * 12.011 / 55.845; // = 0.3226 kg C / kg Fe

struct BloomResult {
    Substance bloom;           // metallic iron (freegrain[IRON])
    double slag[N_ELEM] {};    // element masses locked in the slag
    double gas[N_ELEM] {};     // element masses leaving as CO/CO2
    double bloom_iron = 0.0;   // kg of metal, convenience
    double bloom_sulfur = 0.0; // kg sulfur dissolved in the metal (the red-short seed)
    bool lit = false;          // did the furnace reach reducing conditions
};

// Fire the bloomery. `ore` is the charge, `charcoal` its fuel/reductant (a
// Substance whose carbon is what counts). `reducing` is the tabulated envelope:
// hot enough and CO-rich enough to reduce and to melt fayalite (~1150-1300 C).
// If it is false, nothing reduces and the whole charge is slag.
//
// The ledger balances by construction: every element in ore+charcoal leaves in
// exactly one of bloom, slag, gas. The test asserts it anyway, because a
// bookkeeping error here would be silent.
inline BloomResult bloomery(const Substance& ore, const Substance& charcoal,
                            bool reducing = true) {
    BloomResult r;
    r.lit = reducing;

    double in_ore[N_ELEM], in_fuel[N_ELEM];
    assay_elements(ore, in_ore);
    assay_elements(charcoal, in_fuel);
    double in[N_ELEM];
    for (int e = 0; e < N_ELEM; ++e) in[e] = in_ore[e] + in_fuel[e];

    // Reducible iron, and the oxygen bound to it (the only oxygen that can leave
    // as gas). Non-reducible iron and its oxygen stay in the slag.
    double red_fe = 0.0, red_o = 0.0;
    for (int p = 0; p < N_PHASE; ++p)
        if (reducible(p)) {
            const double m = ore.phase_mass(p);
            red_fe += m * element_fraction(p, EL_FE);
            red_o  += m * element_fraction(p, EL_O);
        }

    if (!reducing) red_fe = 0.0; // a cold furnace reduces nothing

    // The fayalite wall. All silica reports to slag and fluxes iron at 1.859:1.
    const double sio2 = in[EL_SI] * SIO2_PER_SI;
    const double fe_for_slag = std::fmin(red_fe, FE_PER_SIO2 * sio2);

    // Iron that could become metal, after the slag takes its cut.
    double bloom_fe = red_fe - fe_for_slag;

    // Carbon gate: reduction needs CO. If the fuel is short, only that much iron
    // reduces; the rest stays as oxide in the slag.
    const double fe_that_fuel_allows = in[EL_C] / CARBON_PER_FE;
    if (bloom_fe > fe_that_fuel_allows) bloom_fe = fe_that_fuel_allows;
    if (bloom_fe < 0.0) bloom_fe = 0.0;

    r.bloom_iron = bloom_fe;
    r.bloom.freegrain[IRON][SAND] = bloom_fe;
    r.bloom.temperature = ore.temperature;

    // Sulfur partitions. Only a furnace that made metal can take sulfur into it;
    // the acidic slag keeps the rest. This is the seed of red-short, carried in the
    // metal (a scalar, not a slag phase) all the way to the anvil in forge.h.
    const double s_to_metal = (bloom_fe > 0.0) ? in[EL_S] * SULFUR_TO_METAL : 0.0;
    r.bloom_sulfur = s_to_metal;

    // What leaves up the stack. The metal took `bloom_fe` of the reducible iron;
    // the oxygen bound to that fraction goes with it as gas. And all the charcoal
    // is gone -- carbon does not survive a bloomery: what did not reduce iron
    // burned in the blast (whose oxygen is atmospheric and outside this charge
    // ledger, which is why the gas here is not stoichiometric CO2 and does not
    // need to be).
    const double o_to_gas = (red_fe > 0.0) ? red_o * (bloom_fe / red_fe) : 0.0;

    r.gas[EL_O] = o_to_gas;
    r.gas[EL_C] = in[EL_C];

    // Everything the gas and bloom did not take is slag.
    r.slag[EL_FE]    = in[EL_FE] - bloom_fe;
    r.slag[EL_SI]    = in[EL_SI];
    r.slag[EL_O]     = in[EL_O] - o_to_gas;
    r.slag[EL_C]     = 0.0;
    r.slag[EL_S]     = in[EL_S] - s_to_metal;
    r.slag[EL_OTHER] = in[EL_OTHER];

    return r;
}

// The smeltability of a charge, in one number: its iron-to-silica mass ratio.
// Above FE_PER_SIO2 (1.859) a bloom is possible; below it, never.
inline double iron_to_silica(const Substance& s) {
    double el[N_ELEM];
    assay_elements(s, el);
    const double sio2 = el[EL_SI] * SIO2_PER_SI;
    return sio2 > 0.0 ? el[EL_FE] / sio2 : (el[EL_FE] > 0.0 ? INFINITY : 0.0);
}

// ------------------------------------------------------------------------
// THE SECOND METAL: copper, and the divergence at the furnace.
//
// Everything above this line is iron. Copper shares the whole front of the chain
// with it -- you dig it, you concentrate it, you reduce its oxide with charcoal in
// the same kind of hearth -- and then, at the furnace, the two metals part ways.
// The thing that parts them is not a choice a smith makes. It is two melting points
// against one fire, and it is the whole finding of copper:
//
//   THE FURNACE STRADDLES THE TWO MELTING POINTS. A charcoal-and-bellows hearth
//   runs its reduction at ~1150-1300 C. Copper melts at 1085 C, iron at 1538 C. So
//   the IDENTICAL fire clears copper's melting point and falls short of iron's:
//   copper pours out a LIQUID and is cast, iron stays a SOLID sponge and is forged.
//   Nobody chose that. The melting points chose it. And it is why copper metallurgy
//   is thousands of years older than iron's -- not because copper is the commoner
//   metal (iron is far commoner in the crust) but because copper's melting point
//   was the one a wood fire could reach. ABUNDANCE IS NOT THE GATE; MELTING POINT IS.
//
// That state -- liquid vs solid at furnace heat -- is the copper analogue of the
// fayalite wall: a fact forced by two tabulated numbers, not an authored knob. And
// it decides more than cast-vs-forge. Because it pours liquid, molten copper and
// molten slag separate by density in the hearth -- so the cast billet freezes
// essentially slag-free, escaping the connectivity floor that dooms wrought iron to
// stringers (forge.h). That floor was never really about iron; it was about the
// metal being SOLID at furnace heat. Cast from a liquid, copper is clean.
//
// Two smaller divergences ride along. Copper is NOT eaten by its own silica: there
// is no cupric fayalite consuming the metal 1.859:1, so a copper charge has no wall
// of the iron kind -- its yield is gated by reduction alone. And copper is
// diamagnetic (phase_table.h): the lodestone that gates iron ore cannot touch
// copper ore, which is why copper dressing stayed gravity-and-hand-sorting.
//
// Per DESIGN.md's fidelity ceiling: the melting points and the reduction reaction
// are VERIFIED/tabulated; the reduction-ONSET temperature is AUTHORED (a cited
// envelope, issue #25). The finding rides the furnace heat sitting between the two
// melting points, not on any of the rates.

// Melting points, the numbers the furnace reads to decide what STATE its metal
// comes out in. VERIFIED (IUPAC/CRC standard values). TM_IRON also anchors the
// recrystallization floor in forge.h (a fraction of it) -- one number, read by
// both the furnace and the anvil.
inline constexpr double TM_IRON   = 1811.0;   // 1538 C
inline constexpr double TM_COPPER = 1357.77;  // 1084.62 C

// Copper's reducible ore: the oxides that carbon strips to metal. Cuprite (Cu2O)
// stands for them; malachite, the green carbonate, calcines to the same copper
// oxide before it reduces. (Native copper and the sulfides are other entry points;
// this chain takes the oxide, the Bronze-Age one.) There is deliberately no
// cupric-fayalite term -- copper is not consumed by silica the way iron is.
inline bool reducible_cu(int p) { return p == CUPRITE; }

// Carbon to reduce cuprous oxide to metal: Cu2O + C -> 2 Cu + CO, so half a carbon
// per copper, A_C/(2*A_CU) = 0.0945 kg C per kg Cu. DERIVED -- atomic weights, the
// same kind of number as CARBON_PER_FE.
inline constexpr double CARBON_PER_CU = 12.011 / (2.0 * 63.546); // = 0.0945 kg C / kg Cu

// Reduction onset: a copper hearth needs to be hot and CO-rich, ~800 C, before the
// oxide gives up its oxygen. AUTHORED, cited envelope (issue #25). It sits well
// below copper's melting point, so a furnace hot enough to MELT copper is always
// hot enough to REDUCE it -- which is why the molten pour is the ordinary outcome.
inline constexpr double CU_REDUCTION_T = 1073.0; // ~800 C

struct MeltResult {
    Substance metal;            // freegrain[COPPER]: the reduced copper
    double slag[N_ELEM] {};     // element masses in the slag
    double gas[N_ELEM] {};      // C and O leaving as CO/CO2
    double metal_cu = 0.0;      // kg copper reduced
    bool molten = false;        // did the furnace clear copper's melting point
    bool lit = false;           // reducing conditions reached
    double temperature = 288.0; // K, the furnace heat this pour was run at
};

// Fire the copper furnace at `furnace_T` kelvin. The ONE temperature answers two
// questions the finding turns on: is it hot enough to REDUCE (>= CU_REDUCTION_T),
// and hot enough to MELT (>= TM_COPPER, => cast, not sponge). The ledger balances
// by construction: every element in ore+charcoal leaves in exactly one of metal,
// slag, gas.
inline MeltResult smelt_copper(const Substance& ore, const Substance& charcoal,
                               double furnace_T) {
    MeltResult r;
    r.temperature = furnace_T;
    r.lit    = furnace_T >= CU_REDUCTION_T;
    r.molten = furnace_T >= TM_COPPER;

    double in_ore[N_ELEM], in_fuel[N_ELEM];
    assay_elements(ore, in_ore);
    assay_elements(charcoal, in_fuel);
    double in[N_ELEM];
    for (int e = 0; e < N_ELEM; ++e) in[e] = in_ore[e] + in_fuel[e];

    // Reducible copper, and the oxide oxygen bound to it (the only oxygen that can
    // leave as gas). Cuprite is the whole reducible set; its oxygen is all oxide.
    double red_cu = 0.0, red_o = 0.0;
    for (int p = 0; p < N_PHASE; ++p)
        if (reducible_cu(p)) {
            const double m = ore.phase_mass(p);
            red_cu += m * element_fraction(p, EL_CU);
            red_o  += m * element_fraction(p, EL_O);
        }
    if (!r.lit) { red_cu = 0.0; red_o = 0.0; }

    // Carbon gate: reduction needs CO from the FUEL. (The ore's own carbon, if any,
    // is carbonate -- already oxidised -- and cannot reduce.) No fayalite wall here:
    // copper is not fluxed away by silica, so nothing between reduction and metal.
    double metal_cu = red_cu;
    const double cu_that_fuel_allows = in_fuel[EL_C] / CARBON_PER_CU;
    if (metal_cu > cu_that_fuel_allows) metal_cu = cu_that_fuel_allows;
    if (metal_cu < 0.0) metal_cu = 0.0;

    r.metal_cu = metal_cu;
    r.metal.freegrain[COPPER][SAND] = metal_cu;
    r.metal.temperature = furnace_T;

    // The oxide oxygen of the copper that actually reduced leaves as gas, carried
    // off by fuel carbon. All charcoal is gone regardless (as in the bloomery).
    const double o_to_gas = (red_cu > 0.0) ? red_o * (metal_cu / red_cu) : 0.0;
    r.gas[EL_O] = o_to_gas;
    r.gas[EL_C] = in[EL_C];

    // Everything the metal and gas did not take is slag: silica, iron-bearing
    // gangue, unreduced copper oxide, and the rest.
    r.slag[EL_FE]    = in[EL_FE];
    r.slag[EL_SI]    = in[EL_SI];
    r.slag[EL_O]     = in[EL_O] - o_to_gas;
    r.slag[EL_C]     = 0.0;
    r.slag[EL_S]     = in[EL_S];
    r.slag[EL_CU]    = in[EL_CU] - metal_cu;
    r.slag[EL_OTHER] = in[EL_OTHER];

    return r;
}

} // namespace wrought
