#pragma once

// Mineral phase table — Phase A fixture.
//
// Per DESIGN.md §1a every physical number carries a `source`. Nothing here has
// been checked against a reference, so every row ships UNVERIFIED.
//
// The Phase A tests are invariant to these values. They assert relationships —
// conservation, monotonicity, dominance, ceilings — that hold for any densities
// which merely differ from one another. Correcting a number in this file must
// not change a single test outcome. If it does, the test was measuring the
// fixture instead of the model, and the test is wrong.
//
// Note which columns are here: density, and magnetic susceptibility for the
// lodestone. DESIGN.md §1 lists eleven more. They are absent because no process
// implemented yet reads them. A column earns its citation by being load-bearing.

namespace wrought {

enum PhaseId {
    QUARTZ, FELDSPAR, KAOLINITE, MAGNETITE, HEMATITE,
    GOETHITE, ILMENITE, PYRITE, CALCITE, CARBON,
    N_PHASE
};

// Particle-size bins.
//
// These were once three, and the coarsest three are still the ones a *pan*
// distinguishes: mud washes over the lip, sand stratifies, gravel is picked off
// by hand. The pan cannot tell clay from silt and does not need to.
//
// Levigation can, and must. *(Split 2026-07-09.)* The old `FINES` bin ran from
// 3.9 um to 62.5 um and stood for "clay and silt" — but clay and silt are
// exactly the two things levigation separates, and one bin has one settling
// velocity, so the model could not perform the process the design's own ratchet
// is built on. It stalled at 60% kaolinite forever. The boundary that fixes it
// is 3.9 um, which was already in the code as the bottom edge of `FINES`, and
// which is the same Wentworth scale everything else here comes from. No new
// citation was needed to see it; only something that tried to use it.
enum SizeBin { CLAY, SILT, SAND, GRAVEL, N_SIZE };

struct Phase {
    const char* id;
    double density;                 // g/cm^3
    double magnetic_susceptibility; // volume SI, dimensionless
    const char* source;
};

inline constexpr Phase PHASES[N_PHASE] = {
    {"quartz",    2.65, -1.0e-5, "UNVERIFIED"},
    {"feldspar",  2.56, -1.0e-5, "UNVERIFIED"},
    {"kaolinite", 2.60, -1.0e-5, "UNVERIFIED"},
    {"magnetite", 5.15,  3.0,    "UNVERIFIED"},
    {"hematite",  5.26,  1.0e-3, "UNVERIFIED"},
    {"goethite",  4.28,  1.0e-3, "UNVERIFIED"},
    {"ilmenite",  4.72,  2.0e-3, "UNVERIFIED"},
    {"pyrite",    5.01,  1.0e-4, "UNVERIFIED"},
    {"calcite",   2.71, -1.0e-5, "UNVERIFIED"},
    {"carbon",    1.50, -1.0e-5, "UNVERIFIED"},
};

} // namespace wrought
