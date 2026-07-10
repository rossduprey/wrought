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
    IRON,
    CUPRITE, COPPER,
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

// The third column is the grain's shape, and it is the one that is easy to
// misread. `aspect_ratio` is p = (polar semiaxis / equatorial semiaxis) of the
// oblate spheroid that stands for a grain of this phase. p = 1 is a sphere.
// p = 0.1 is a plate ten times wider than it is thick.
//
// `settling.h` reads it and nothing else does. It buys two things a sphere
// cannot: the Stokes shape factor, which is exact, and the volume-equivalent
// diameter, which is what a size bin's diameter is NOT. See the long comment
// there — the difference between those two readings of a bin is a factor of
// seven on kaolinite, and it decides whether Era 1 needs deflocculation.
//
// Every p = 1.0 here is AUTHORED and every one of them is a lie of convenience:
// quartz sand is blocky (Wadell sphericity ~0.7), goethite is acicular,
// specular hematite is a plate. They are 1.0 because no process yet
// distinguishes them and because a wrong number that is *stated* is worse than
// an idealisation that is *labelled*. The one that is not 1.0 is kaolinite, and
// it is not 1.0 because the entire point of clay is that it is not a marble.
//
// The direction of the p=1 error is known: giving quartz its real sphericity of
// 0.8 slows it ~7%, which *narrows* the kaolinite/quartz velocity gap from
// 6.98x to 6.47x. The finding does not depend on it. Issue: authored-number.
// A habit has a size, and forgetting that was a defect this table shipped for
// about an hour. Aspect ratio is a property of a CRYSTAL, and kaolinite crystals
// are 0.1-4.0 um across — which is to say they exist in the CLAY bin and nowhere
// else. Applied at every size it produced an 11 mm platelet 1.1 mm thick, which
// is not a mineral, and a pan test caught it. Coarser kaolinite is an aggregate
// of platelets stacked into a lump, and a lump is blocky.
//
// So `platy_below` is the face diameter above which the habit stops and the
// grain reverts to a sphere. For kaolinite it is 4.0 um: the top of the measured
// range, and — not by our arrangement — the top of the CLAY bin.
struct Phase {
    const char* id;
    double density;                 // g/cm^3
    double magnetic_susceptibility; // volume SI, dimensionless
    double aspect_ratio;            // p = c/a of the equivalent oblate spheroid
    double platy_below;             // m; above this face diameter, a grain is a lump
    const char* source;
    const char* shape_source;
};

inline constexpr Phase PHASES[N_PHASE] = {
    {"quartz",    2.65, -1.0e-5, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"feldspar",  2.56, -1.0e-5, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    // Kaolinite platelets: face diameter 0.1-4.0 um, aspect ratio typically ~10,
    // ranging 2 (stacked books) to ~36 (single flakes). Ndlovu et al., "A new
    // method for determining platy particle aspect ratio: A kaolinite case
    // study", Applied Clay Science 92 (2014) 23-28; Zhang et al., Materials
    // Letters 158 (2015) 236-239. AUTHORED in the same sense bin_diameter is:
    // a point taken from a cited distribution, not a number chosen to make
    // anything come out right. The distribution is wide and the model is
    // linear in p^(2/3), so this is a tracked issue, not a fact.
    {"kaolinite", 2.60, -1.0e-5, 0.10, 4.0e-6, "UNVERIFIED", "AUTHORED point, cited range"},
    {"magnetite", 5.15,  3.0,    1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"hematite",  5.26,  1.0e-3, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"goethite",  4.28,  1.0e-3, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"ilmenite",  4.72,  2.0e-3, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"pyrite",    5.01,  1.0e-4, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"calcite",   2.71, -1.0e-5, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    {"carbon",    1.50, -1.0e-5, 1.00, 0.0, "UNVERIFIED", "AUTHORED isometric"},
    // Metallic iron: not a mineral in the ground but a *product* -- the bloom
    // that falls out of the bloomery (smelt.h). It exists in this table so a bloom
    // has somewhere to live and forging has something to pick up. Density is
    // alpha-iron, a standard figure. Susceptibility is ferromagnetic and enormous;
    // 200 is a placeholder that only has to be "the lodestone grabs a bloom", which
    // it should. AUTHORED. It is inert to every separator here except the magnet.
    {"iron",      7.87,  2.0e2,   1.00, 0.0, "UNVERIFIED alpha-Fe", "AUTHORED lump"},
    // Cuprite, Cu2O ("ruby copper"): a clean cuprous-oxide copper ore. Malachite,
    // the green carbonate everyone pictures starting the Bronze Age, calcines to
    // the same reducible copper oxide on the way up to heat; cuprite is modelled
    // because its oxygen ledger is exact (no carbonate/hydroxide volatiles to
    // split), and the finding does not care which oxide the carbon strips. Density
    // ~6.0 (cuprite is denser than the silicates); diamagnetic-to-weakly-para, and
    // in any case NOT ferromagnetic -- the lodestone cannot concentrate copper ore,
    // which is why copper dressing was gravity and hand-sorting, never the magnet.
    {"cuprite",   6.00, -1.0e-5, 1.00, 0.0, "UNVERIFIED Cu2O", "AUTHORED isometric"},
    // Metallic copper: the cast product, copper's answer to the iron bloom. It
    // exists here so a pour has somewhere to live and shaping something to pick up.
    // Density is standard copper. Susceptibility is diamagnetic -- copper is the
    // textbook diamagnet, and unlike an iron bloom a copper billet is invisible to
    // the lodestone. AUTHORED. Inert to every separator here, magnet included.
    {"copper",    8.96, -1.0e-5, 1.00, 0.0, "UNVERIFIED Cu", "AUTHORED lump"},
};

} // namespace wrought
