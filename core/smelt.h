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

namespace wrought {

// Elements we account. OTHER lumps Ti, S, Ca, Al, K, H, Mn... -- everything not
// on the iron/silica/carbon/oxygen ledger. It is conserved as a bucket, which is
// all the balance needs.
enum Element { EL_FE, EL_SI, EL_O, EL_C, EL_OTHER, N_ELEM };

// A mineral's formula, as counts of the tracked atoms per formula unit plus the
// molar mass. Everything else in the formula falls into OTHER = M - tracked.
// Atomic weights are IUPAC 2021 standard values -- the most citable numbers in
// the project, and the reason nothing in this file is UNVERIFIED.
struct Formula { double molar_mass, n_fe, n_si, n_o, n_c; };

namespace detail {
    constexpr double A_FE = 55.845, A_SI = 28.085, A_O = 15.999, A_C = 12.011;
    // Per-phase formulae, indexed by PhaseId. A zero row means "no tracked atoms",
    // which is correct for a bloom of metallic iron carrying only Fe.
    inline constexpr Formula FORMULA[N_PHASE] = {
        /*QUARTZ    SiO2         */ {60.083,  0, 1, 2, 0},
        /*FELDSPAR  KAlSi3O8     */ {278.33,  0, 3, 8, 0},
        /*KAOLINITE Al2Si2O5(OH)4*/ {258.16,  0, 2, 9, 0},
        /*MAGNETITE Fe3O4        */ {231.53,  3, 0, 4, 0},
        /*HEMATITE  Fe2O3        */ {159.69,  2, 0, 3, 0},
        /*GOETHITE  FeO(OH)      */ {88.851,  1, 0, 2, 0},
        /*ILMENITE  FeTiO3       */ {151.71,  1, 0, 3, 0},
        /*PYRITE    FeS2         */ {119.97,  1, 0, 0, 0},
        /*CALCITE   CaCO3        */ {100.09,  0, 0, 3, 1},
        /*CARBON    C            */ {12.011,  0, 0, 0, 1},
        /*IRON      Fe (metal)   */ {55.845,  1, 0, 0, 0},
    };
}

// Mass fraction of element e in phase p.
inline double element_fraction(int p, int e) {
    const Formula& f = detail::FORMULA[p];
    const double tracked = f.n_fe * detail::A_FE + f.n_si * detail::A_SI
                         + f.n_o * detail::A_O + f.n_c * detail::A_C;
    switch (e) {
        case EL_FE:    return f.n_fe * detail::A_FE / f.molar_mass;
        case EL_SI:    return f.n_si * detail::A_SI / f.molar_mass;
        case EL_O:     return f.n_o  * detail::A_O  / f.molar_mass;
        case EL_C:     return f.n_c  * detail::A_C  / f.molar_mass;
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
// and reports to slag; pyrite is a sulfide and poisons the bloom, not reduces to
// it. So the reducible set is exactly the three iron oxides. Issue #21.
inline bool reducible(int p) { return p == MAGNETITE || p == HEMATITE || p == GOETHITE; }

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
    Substance bloom;         // metallic iron (freegrain[IRON])
    double slag[N_ELEM] {};  // element masses locked in the slag
    double gas[N_ELEM] {};   // element masses leaving as CO/CO2
    double bloom_iron = 0.0; // kg of metal, convenience
    bool lit = false;        // did the furnace reach reducing conditions
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

} // namespace wrought
