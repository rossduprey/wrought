#pragma once

#include "fuel.h"

// The fire kit and the spark: the station between the gathered wood and every fire
// that burns it, and the one tool without which none of it lights.
//
// fuel.h established WHAT wood is (tinder/sticks/timber) and the two gates a fire
// has once lit (reductant, peak temperature). It left one thing assumed that this
// file makes real: the SPARK. fuel.h's can_light() returns true on dry tinder alone
// -- as if a fire lights itself the moment the tinder is dry enough. It does not.
// John Plant lights every fire the same way, and it is not with a match found in the
// grass: it is with a friction set he MADE. The spark is a technology, the first one,
// and the model spawned it. This file stops spawning it.
//
//   THE FUEL DOES NOTHING WITHOUT THE PLAYER, AND THE PLAYER DOES NOTHING WITHOUT
//   THE SPARK.
//
// Two structural facts, and this file authors no number for either:
//
//   1. THE IGNITION LADDER. No spark catches a log; a spark catches tinder, tinder
//      lights sticks, sticks light split timber, and a sustained fire feeds every
//      rung at once. So a kit's fire is only ever as high as its lowest MISSING
//      rung: timber in a kit with no tinder catches nothing, because nothing lights
//      it. The ladder is strict and bottom-up -- no rung skipped -- and it is why a
//      smelt-grade timber fire is locked until the axe frees timber, exactly as the
//      pick's reach is locked behind the whole tool staircase. New tech adds a rung;
//      it never retires the ones beneath. This is an ORDERING over rungs the player
//      either has or has not gathered; there is no magnitude in it.
//
//   2. THE SMOLDER KIT IS A TOOL, NOT A FUEL. It is the friction/bow-drill set every
//      fire starts with -- made once, from dry dead wood already gathered, and then
//      PERSISTENT. It does not wear: firemaking mastery is time-compression the sim
//      hides so the player has fun, not labour, the same gloss as the panner's wrist
//      and the axe swing. Possession is the whole of it -- pick-shaped, like the
//      pick's reach -- and possession is the gate. A fire station loaded with a
//      perfect kit is INERT storage until the player strikes it with the smolder
//      tool. The strike is a deliberate action (a click, a key), never a passive
//      burn; the fuel waits on the player, and the player waits on the spark.
//
// Scope (design decision, 2026-07-12): like split.h and char.h, this file is a
// STRUCTURAL station, not a combustion or ergonomics model. It authors no constant:
// the ladder reads rung PRESENCE, and the dryness both the tinder and the friction
// set turn on is fuel.h's own TINDER_MOISTURE_MAX (already issue #32). The mass a
// kit weighs is just its rungs summed, hauled under carry.cpp's existing carrier
// caps -- so the carry limit is slice-local pacing there, not a number here. The
// teaching strings the ladder's failures earn ("where is the spark?") are authored
// in the SLICE, downstream, the same place gather.cpp's reckoning lives.

namespace wrought {

// ---------------------------------------------------------------------------
// The kit staged at a station IS a bundle of gathered wood -- the same tinder,
// sticks, timber, moisture the player won at the clearing and hauled here. Nothing
// is created assembling it, so it is not a new kind of matter; it is fuel.h's Wood,
// named for the role it now plays. Its haul mass is exactly its rungs, the mass the
// carry slice plays against its carrier caps.
using FireKit = Wood;
inline double kit_mass(const FireKit& k) { return k.tinder + k.sticks + k.timber; }

// ---------------------------------------------------------------------------
// The ignition ladder. A fire is only as high as the tallest UNBROKEN run of rungs
// from the bottom: each rung is what lights the next, so a gap stops the climb. This
// authors nothing -- it reads presence (a rung is in the kit or it is not) and the
// bottom rung's dryness gate is fuel.h's TINDER_MOISTURE_MAX (issue #32).
enum FireTier {
    NO_FIRE,       // no dry tinder: no spark catches. The ladder has no bottom rung.
    TINDER_FLARE,  // tinder only: it flares and is gone. Starts a fire; is not one.
    STICK_FIRE,    // + sticks: a sustained hand-fuel fire. Carries a copper reduction.
    TIMBER_FIRE    // + split timber: the bulk fire a pit or bloom wants. Axe-gated.
};

inline FireTier fire_tier(const FireKit& k) {
    if (!(k.tinder > 0.0 && k.moisture < TINDER_MOISTURE_MAX)) return NO_FIRE;
    if (k.sticks <= 0.0) return TINDER_FLARE;
    if (k.timber <= 0.0) return STICK_FIRE;
    return TIMBER_FIRE;
}

// ---------------------------------------------------------------------------
// The smolder kit: the spark, made once and kept. A TOOL -- possession is the whole
// of it, and it does not wear (mastery is glossed, like the panner's tilt). It is
// carved from dry dead wood; a green stick takes no ember, the same dryness gate the
// tinder turns on. There is no mass here to spend at the model level -- how much
// stock a set costs, if anything, is slice-local pacing, not a finding.
struct SmolderKit {
    bool made = false;   // pick-shaped: having it is the gate, not any wear on it.
};

inline SmolderKit make_smolder(const Wood& stock) {
    SmolderKit s;
    s.made = stock.sticks > 0.0 && stock.moisture < TINDER_MOISTURE_MAX;
    return s;
}

// ---------------------------------------------------------------------------
// A fire station in one of three honest states, every transition driven by the
// player: EMPTY (no kit -- nothing to burn), LOADED (a kit sits in it, INERT --
// storage, the fuel doing nothing on its own), LIT (the player struck it and it
// caught). Nothing here burns passively; the enum is what a slice draws and what
// ignite() moves between.
enum StationState { EMPTY, LOADED, LIT };

// The ignition ACTION. This is where fuel.h's can_light() is corrected: the spark is
// not free, it is the tool. A loaded station stays inert until struck, and the
// strike needs the smolder kit in hand -- no kit, no fire, however dry the tinder.
// With the kit, the fire catches to the highest rung the ladder reaches. Two honest
// failures fall out: no tool (where is the spark?), and tool-but-no-dry-tinder
// (nothing catches) -- the slice names them.
struct Ignition {
    bool     lit  = false;
    FireTier tier = NO_FIRE;
};

inline Ignition ignite(const FireKit& kit, const SmolderKit& tool) {
    Ignition r;
    if (!tool.made) return r;          // loaded, but no spark: inert storage.
    r.tier = fire_tier(kit);           // and it still needs dry tinder to catch.
    r.lit  = r.tier != NO_FIRE;
    return r;
}

} // namespace wrought
