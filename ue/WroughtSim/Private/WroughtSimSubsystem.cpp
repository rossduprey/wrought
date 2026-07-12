#include "WroughtSimSubsystem.h"

// The wrought sim. Header-only C++17, all of it confined to this .cpp so Unreal Header
// Tool never sees the namespace. geology.h pulls substance/settling; haft.h pulls
// knap.h -- these two umbrellas cover the whole seam.
#include "geology.h"
#include "haft.h"

namespace
{
    // Render one bite of ground for the panel. This is the only place sim types cross
    // into Unreal types. Free mass is what a pan takes at the face; locked mass is the
    // COMPOSITE_TARGET_FRACTION of composite grains -- the ore trapped behind the breaker.
    FWroughtBite Render(const wrought::Substance& s, wrought::Place at)
    {
        using namespace wrought;
        FWroughtBite out;
        out.Place = FVector2D(static_cast<float>(at.x), static_cast<float>(at.y));
        out.TotalMassKg = static_cast<float>(s.total_mass());
        out.bWonRock = out.TotalMassKg > 0.f;   // win_bite returns an empty Substance when it skips off

        constexpr double f = COMPOSITE_TARGET_FRACTION;
        for (int p = 0; p < N_PHASE; ++p)
        {
            double freeMass = 0.0, lockedTarget = 0.0;
            for (int b = 0; b < N_SIZE; ++b)
            {
                freeMass     += s.freegrain[p][b];
                lockedTarget += f * s.composite[p][b];   // the target phase locked in composite
            }
            if (freeMass + lockedTarget <= 0.0) continue;

            FWroughtPhaseReadout row;
            row.Phase        = ANSI_TO_TCHAR(PHASES[p].id);
            row.Grade        = static_cast<float>(s.grade(p));
            row.FreeMassKg   = static_cast<float>(freeMass);
            row.LockedMassKg = static_cast<float>(lockedTarget);
            out.Phases.Add(row);
        }
        return out;
    }
}

FVector2D UWroughtSimSubsystem::WorldToPlace(FVector WorldLocation) const
{
    // The whole spatial contract. Drop the z -- depth is a tier, not a coordinate.
    const float inv = (UnitsPerMeter != 0.f) ? (1.f / UnitsPerMeter) : 0.f;
    return FVector2D(
        (WorldLocation.X - PlaneOrigin.X) * inv,
        (WorldLocation.Y - PlaneOrigin.Y) * inv);
}

FWroughtBite UWroughtSimSubsystem::BiteAt(FVector WorldLocation)
{
    const FVector2D pl = WorldToPlace(WorldLocation);
    const wrought::Place at{ pl.X, pl.Y };
    const int tier = FMath::Clamp(SelectedTier, 0, wrought::N_TIER - 1);

    const wrought::Substance s =
        wrought::win_bite(at, tier, BiteMassKg, BlowEnergy);
    return Render(s, at);
}

FWroughtBite UWroughtSimSubsystem::DigColumnAt(FVector WorldLocation)
{
    const FVector2D pl = WorldToPlace(WorldLocation);
    const wrought::Place at{ pl.X, pl.Y };

    // The tool-gated overload: each tier is won only if the blow gets under its rock.
    const wrought::Substance s =
        wrought::dig_column(at, BiteMassKg, BlowEnergy);
    return Render(s, at);
}

void UWroughtSimSubsystem::EquipBareHand()
{
    using namespace wrought;
    // A representative flake in the fist: hand_bite is swing_energy at HAND_REACH with a
    // grip that never slips. Clears COMPETENCE_SURFACE (0.001) and a placer (0.0), skips
    // MIDDLE/DEEP -- the bare-hand finding, taken from the model, not typed in.
    const StoneEdge flake{ /*mass*/ 0.2, /*edge_angle*/ 70.0, /*usable*/ true };
    BlowEnergy   = static_cast<float>(hand_bite(flake));
    BiteMassKg   = 0.2f;   // a little, just to read the panel
}

void UWroughtSimSubsystem::EquipSaplingPick()
{
    using namespace wrought;
    // The crude bootstrap pick: a knapped point seated on a hand-cut sapling. Its bite()
    // clears COMPETENCE_DEEP (0.100), so it wins every tier -- STATUS.md's "even a crude
    // sapling pick wins every tier". Head mass and haft length are player-equipment
    // placeholders; the joint and energy come from haft.h.
    const StoneEdge head{ /*mass*/ 0.8, /*edge_angle*/ 60.0, /*usable*/ true };
    const Hafted pick = haft(head, /*haft_len*/ 0.6, SAPLING, BIND_SEATED, HEAD_POINT);
    BlowEnergy   = static_cast<float>(pick.bite());
    BiteMassKg   = 2.0f;   // a pick wins more of the same makeup than a hand's read
}
