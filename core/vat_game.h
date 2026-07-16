#pragma once

// The vat, without a terminal -- the levigation session as a playable station.
//
// This is pan_game.h's sibling, and their difference IS the lesson. The pan's game
// is continuous: the cut is the hand's instantaneous water speed, decisions are
// dense, and it runs 1:1. Levigation is the opposite temperament: stir (seconds),
// then the settle is HOURS with exactly one decision inside it -- when to pour.
// Pour early and the liquor is mud (silt riding with the clay, high recovery, low
// grade); wait and it cleans itself, but past the silt the clay starts going down
// too and every hour costs recovery. Grade trades against recovery, held as
// patience. Per DESIGN.md's compression law the wait between decisions compresses
// (settle_rate, a pacing knob, never a sim number) and the pour is 1:1.
//
// Same discipline as pan_game.h: the physics has exactly one home (levigate.h --
// decant() is called, never reimplemented), feedback is EMITTED for a host to
// drain, and nothing here authors a number the sim does not already own. The one
// authored constant is VAT_CHARGE_FRACTION (how full of mud a working vat can be),
// which is a statement about needing water above the sediment, not about minerals.
//
// The session never "ends": the vat is a world station. fill() takes a Substance
// from the host (the basket the player carried); pour() hands the liquor BACK to
// the host (the clay won -- bank it); the sediment stays in the vat for another
// stir or a dump(). Same rule as the pan seam: this class owns no world state,
// it is a lens the player holds over a Substance in transit through the graph.

#include <cstdio>
#include <string>
#include <vector>

#include "levigate.h"

namespace wrought {

// Fixed step, matching the pan's cadence so one host loop drives both.
inline constexpr double VAT_DT = 0.05;   // s per tick

// How fast the settle clock runs against real time. PACING, not physics: the
// decision is WHEN to pour, and the compression law says the featureless wait
// between decisions compresses freely. At HOLLOW's 0.05 m depth the honest
// window (sand down ~15 s, silt down ~2-3 min) becomes a playable half-minute
// at 8x. A host may set it 1.0 to feel the real bank-side wait. AUTHORED.
inline constexpr double VAT_SETTLE_RATE = 8.0;

// A working vat is mud UNDER water; fill it solid and there is nothing to pour.
// Cap the charge at this fraction of the mass whose settled bed would exactly
// fill the vessel (BULK_DENSITY * volume). AUTHORED.
inline constexpr double VAT_CHARGE_FRACTION = 0.5;

inline double vat_charge_cap(const Vessel& ves) {
    return VAT_CHARGE_FRACTION * BULK_DENSITY * ves.volume();
}

class VatGame {
public:
    VatGame() : vessel_(HOLLOW) {}
    explicit VatGame(const Vessel& v) : vessel_(v) {}

    // --- Inputs (station verbs). --------------------------------------------

    // Tip a load of dirt in. Returns the mass actually accepted (kg); the rest is
    // the caller's to keep -- the vat never destroys what it cannot hold. Filling
    // re-muddies everything: the new dirt joins the charge and the clock resets.
    double fill(const Substance& mud) {
        const double room = vat_charge_cap(vessel_) - charge_.total_mass();
        if (room <= 1e-9) {
            say("The hollow is full of mud already. Pour, or dig it out.");
            return 0.0;
        }
        const double m = mud.total_mass();
        if (m < 1e-9) { say("Your basket has nothing in it."); return 0.0; }
        const double take = m < room ? m : room;
        Substance in = mud;
        if (take < m) {
            const double f = take / m;
            for (int p = 0; p < N_PHASE; ++p)
                for (int s = 0; s < N_SIZE; ++s) {
                    in.freegrain[p][s] *= f;
                    in.composite[p][s] *= f;
                }
        }
        charge_.add(in);
        stirred_ = false;
        t_settle_ = 0.0;
        say(take < m
            ? "You tip in what the hollow will take. The rest stays in the basket."
            : "You tip the basket into the hollow. The water goes brown at once.");
        return take;
    }

    // Stir. Every grain uniform in depth again (levigate.h's premise), the
    // clock at zero. Stirring an empty vat teaches the input, not the strategy.
    void stir() {
        if (charge_.total_mass() < 1e-9) { say("You stir water. It stays water."); return; }
        stirred_ = true;
        t_settle_ = 0.0;
        stage_ = 0;
        say("You drag your hand through it until the whole hollow is one brown cloud.");
    }

    // Pour off whatever is still above the sediment line, into the catch. What
    // leaves is the liquor (your clay, if you timed it); what stays is the
    // sediment, which becomes the new charge for another stir. The returned
    // Substance is the HOST's -- bank it in the basket's clay heap.
    Substance pour() {
        if (charge_.total_mass() < 1e-9) { say("There is nothing in the hollow but water."); return Substance(); }
        if (!stirred_) { say("The mud sits packed on the bottom. Stir it first."); return Substance(); }
        DecantResult d = decant(charge_, vessel_, t_settle_);
        charge_ = d.sediment;
        stirred_ = false;
        const double m = d.liquor.total_mass();
        const double k = d.liquor.phase_mass(KAOLINITE);
        char buf[192];
        if (m < 1e-6) {
            say("You tip the vessel and clear water runs off. Everything had gone down.");
        } else {
            std::snprintf(buf, sizeof(buf),
                          "You pour off the cloud: %.0f g of solids ride out with the water, %.0f g of it clay.",
                          m * 1000.0, k * 1000.0);
            say(buf);
        }
        t_settle_ = 0.0;
        return d.liquor;
    }

    // Shovel the sediment back onto the bank. The vat is a gift; the sand is not.
    void dump() {
        if (charge_.total_mass() < 1e-9) { say("The hollow is already empty."); return; }
        charge_ = Substance();
        stirred_ = false;
        t_settle_ = 0.0;
        say("You scoop the wet sand out onto the bank.");
    }

    // --- The clock. Fixed tick; the settle advances at settle_rate. ---------
    void tick() {
        if (!stirred_ || charge_.total_mass() < 1e-9) return;
        t_settle_ += VAT_DT * settle_rate_;

        // Narrate the water as it clears -- transitions only, never a strategy.
        // Stages are read off the suspension the sim reports, not off timers.
        const int s = stage_now();
        if (s > stage_) {
            stage_ = s;
            switch (s) {
                case 1: say("The grit is down almost as soon as you stop. The cloud stays."); break;
                case 2: say("The water has gone from brown to a pale milk."); break;
                case 3: say("A fine white haze hangs in the water and will not hurry."); break;
            }
        }
    }

    // --- Read-outs for a renderer / HUD. -------------------------------------

    // What a pour RIGHT NOW would carry out -- the decision aid. This is the
    // vat's assay panel: mass over the lip and how much of it is clay.
    void pour_preview(double& out_mass_kg, double& out_clay_kg) const {
        out_mass_kg = out_clay_kg = 0.0;
        if (!stirred_ || charge_.total_mass() < 1e-9) return;
        DecantResult d = decant(charge_, vessel_, t_settle_);
        out_mass_kg = d.liquor.total_mass();
        out_clay_kg = d.liquor.phase_mass(KAOLINITE);
    }

    // 0 = just stirred (everything up), 1 = sand down, 2 = silt going, 3 = clay
    // haze only. Derived from the live suspension, so a shallow fill and a deep
    // one read differently, as they should.
    int stage_now() const {
        if (!stirred_ || charge_.total_mass() < 1e-9) return 0;
        DecantResult d = decant(charge_, vessel_, t_settle_);
        const double up = d.liquor.total_mass();
        if (up < 1e-9) return 3;
        double sandy = 0.0, fine = 0.0;
        for (int p = 0; p < N_PHASE; ++p) {
            sandy += d.liquor.freegrain[p][SAND] + d.liquor.freegrain[p][GRAVEL]
                   + d.liquor.composite[p][SAND] + d.liquor.composite[p][GRAVEL];
            fine  += d.liquor.freegrain[p][SILT] + d.liquor.composite[p][SILT];
        }
        if (sandy / up > 0.05) return 0;   // grit still up: fresh stir
        if (fine / up > 0.30)  return 1;   // sand down, silt riding
        if (fine / up > 0.03)  return 2;   // silt mostly down
        return 3;                          // the clay haze
    }

    // Cloudiness 0..1 for a material/visual: what fraction of the charge is
    // still up. 1 just after a stir, falling as the water clears.
    double cloudiness() const {
        const double m = charge_.total_mass();
        if (!stirred_ || m < 1e-9) return 0.0;
        DecantResult d = decant(charge_, vessel_, t_settle_);
        return d.liquor.total_mass() / m;
    }

    bool   stirred()        const { return stirred_; }
    double settle_seconds() const { return t_settle_; }   // true sim seconds
    double charge_kg()      const { return charge_.total_mass(); }
    double charge_room_kg() const { return vat_charge_cap(vessel_) - charge_.total_mass(); }
    double settle_rate()    const { return settle_rate_; }
    void   set_settle_rate(double r) { settle_rate_ = r > 0.0 ? r : 1.0; }
    const Substance& charge() const { return charge_; }
    const Vessel&    vessel() const { return vessel_; }

    // --- Feedback the caller drains (chat lines). Empties the queue. --------
    std::vector<std::string> drain_feedback() {
        std::vector<std::string> out;
        out.swap(feedback_);
        return out;
    }

private:
    void say(const std::string& s) { feedback_.push_back(s); }

    Vessel vessel_;
    Substance charge_;          // what is in the hollow (mud; sediment after a pour)
    bool stirred_ = false;      // uniform-in-depth? (levigate.h's starting premise)
    double t_settle_ = 0.0;     // true sim seconds since the stir
    double settle_rate_ = VAT_SETTLE_RATE;
    int stage_ = 0;
    std::vector<std::string> feedback_;
};

} // namespace wrought
