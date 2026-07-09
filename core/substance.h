#pragma once

#include "phase_table.h"

// A substance instance: populations of particles, resolved by phase and size.
//
// This is the third version of this struct, and the reasons it changed twice are
// worth keeping.
//
// DESIGN.md originally proposed composition[N_PHASE] + psd[N_SIZE] + a scalar
// liberation. That cannot express what over-processing is supposed to teach,
// because liberation is not a property of a scoop — it is a property of a phase
// at a size. Magnetite in a placer is free in the sand and locked in the gravel.
//
// The obvious repair — mass[phase][size] with liberation[phase][size] — is also
// wrong, and the harness caught it. If liberation is a scalar fraction of a
// cell, then each phase's mass moves through a separator independently, and a
// locked magnetite grain arrives in the concentrate *without the quartz it is
// physically glued to*. Measured, that inflated concentrate grade by ~13% at a
// liberation of 0.5. The grade ceiling the design depends on was not being
// enforced; it was being asserted.
//
// So a composite grain is a particle, not a fraction. It has a mass, it travels
// at its own density, and when it reports to the concentrate it brings its
// gangue with it. Liberation stops being stored and becomes what it always was:
// a ratio you can compute.

namespace wrought {

// Mass fraction of the target phase inside a composite grain. AUTHORED,
// UNVERIFIED. It sets where the grade ceiling sits, not whether there is one.
inline constexpr double COMPOSITE_TARGET_FRACTION = 0.5;

// Composite grains are modelled as target-phase-plus-quartz. Real gangue is a
// silicate mixture; quartz stands for it. UNVERIFIED simplification: a composite
// of two heavy phases (magnetite in ilmenite) is not representable.
inline constexpr int GANGUE = QUARTZ;

struct Substance {
    // Pure grains of phase p in size bin s. Mass, kg.
    double freegrain[N_PHASE][N_SIZE] {};

    // Composite grains whose target phase is p. This is the TOTAL particle mass:
    // COMPOSITE_TARGET_FRACTION of it is phase p, the remainder is gangue.
    // composite[GANGUE][*] is meaningless and stays zero — gangue's locked form
    // is being part of somebody else's composite.
    double composite[N_PHASE][N_SIZE] {};

    double temperature = 288.0; // K
    double moisture = 0.0;      // mass fraction

    double total_mass() const {
        double t = 0.0;
        for (int p = 0; p < N_PHASE; ++p)
            for (int s = 0; s < N_SIZE; ++s) t += freegrain[p][s] + composite[p][s];
        return t;
    }

    // How much of phase p is present, wherever it is hiding.
    double phase_mass(int p) const {
        constexpr double f = COMPOSITE_TARGET_FRACTION;
        double t = 0.0;
        for (int s = 0; s < N_SIZE; ++s) t += freegrain[p][s] + f * composite[p][s];
        if (p == GANGUE)
            for (int q = 0; q < N_PHASE; ++q)
                if (q != GANGUE)
                    for (int s = 0; s < N_SIZE; ++s) t += (1.0 - f) * composite[q][s];
        return t;
    }

    double grade(int p) const {
        const double t = total_mass();
        return t > 0.0 ? phase_mass(p) / t : 0.0;
    }

    // Derived, not stored: the fraction of phase p at size s present as free
    // grains. This is what a separator can actually win.
    double liberation(int p, int s) const {
        constexpr double f = COMPOSITE_TARGET_FRACTION;
        const double total = freegrain[p][s] + f * composite[p][s];
        return total > 0.0 ? freegrain[p][s] / total : 0.0;
    }

    void add(const Substance& o) {
        for (int p = 0; p < N_PHASE; ++p)
            for (int s = 0; s < N_SIZE; ++s) {
                freegrain[p][s] += o.freegrain[p][s];
                composite[p][s] += o.composite[p][s];
            }
    }
};

// Density of a composite grain of phase p: volume mixing with the gangue.
inline double composite_density(int p) {
    constexpr double f = COMPOSITE_TARGET_FRACTION;
    return 1.0 / (f / PHASES[p].density + (1.0 - f) / PHASES[GANGUE].density);
}

} // namespace wrought
