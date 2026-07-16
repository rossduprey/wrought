#include "WroughtSimSubsystem.h"

// The wrought sim. Header-only C++17, all of it confined to this .cpp so Unreal Header
// Tool never sees the namespace. geology.h pulls substance/settling; haft.h pulls
// knap.h -- these two umbrellas cover the whole seam.
#include "geology.h"
#include "haft.h"
#include "levigate.h"   // decant(), HOLLOW, DecantResult -- the second separator
#include "vat_game.h"   // the levigation session, played -- the first gate

// The basket's real contents: two Substance heaps plus the vat session, the sim's own
// types. Defined here (not the header) so no wrought type crosses into the reflected
// surface. TPimplPtr in the header holds one of these; the belt sees only the getters.
struct FWroughtBasket
{
    wrought::Substance dirt;   // raw dug stock, poured in bite by bite
    wrought::Substance clay;   // levigated liquor, accumulated pass by pass
    wrought::VatGame   vat;    // the hollow scraped in the bank (world station state)
};

namespace
{
    // Pour one Substance into another, grain for grain. The basket accumulates; the
    // sim has no operator+= because nothing in core/ needed one until the basket did.
    void AddInto(wrought::Substance& dst, const wrought::Substance& src)
    {
        using namespace wrought;
        for (int p = 0; p < N_PHASE; ++p)
            for (int s = 0; s < N_SIZE; ++s)
            {
                dst.freegrain[p][s] += src.freegrain[p][s];
                dst.composite[p][s] += src.composite[p][s];
            }
    }

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

    // Carry the REAL grain composition into the basket, not just its mass. This is the
    // stock levigation will later separate; a scalar kg could not be decanted.
    if (s.total_mass() > 0.0)
    {
        if (!Basket) Basket = MakePimpl<FWroughtBasket>();
        AddInto(Basket->dirt, s);
    }
    return Render(s, at);
}

FWroughtBite UWroughtSimSubsystem::DigColumnAt(FVector WorldLocation)
{
    const FVector2D pl = WorldToPlace(WorldLocation);
    const wrought::Place at{ pl.X, pl.Y };

    // The tool-gated overload: each tier is won only if the blow gets under its rock.
    const wrought::Substance s =
        wrought::dig_column(at, BiteMassKg, BlowEnergy);

    if (s.total_mass() > 0.0)
    {
        if (!Basket) Basket = MakePimpl<FWroughtBasket>();
        AddInto(Basket->dirt, s);
    }
    return Render(s, at);
}

float UWroughtSimSubsystem::CarriedDirtKg() const
{
    return Basket ? static_cast<float>(Basket->dirt.total_mass()) : 0.f;
}

float UWroughtSimSubsystem::CarriedClayKg() const
{
    return Basket ? static_cast<float>(Basket->clay.total_mass()) : 0.f;
}

FWroughtDecant UWroughtSimSubsystem::Levigate(float Seconds)
{
    using namespace wrought;
    FWroughtDecant out;
    if (!Basket || Basket->dirt.total_mass() <= 0.0)
        return out;   // nothing to pour

    const double t = FMath::Max(0.f, Seconds);

    // The core process, verbatim. Stir the basket's dirt into the authored HOLLOW, wait
    // t, pour off the clear water. liquor is clay; sediment is what was too fast to stay
    // up. The sim decides the cut; we only move the results between basket heaps.
    const DecantResult r = decant(Basket->dirt, HOLLOW, t);

    AddInto(Basket->clay, r.liquor);   // the clay you poured off joins the clay heap
    Basket->dirt = r.sediment;         // the tailings stay as dirt, ready to re-decant

    out.bDecanted       = true;
    out.ClayKg          = static_cast<float>(r.liquor.total_mass());
    out.ClayGrade       = static_cast<float>(r.liquor.grade(KAOLINITE));
    out.TailingsKg      = static_cast<float>(r.sediment.total_mass());
    out.ClayTotalKg     = static_cast<float>(Basket->clay.total_mass());
    out.DirtRemainingKg = static_cast<float>(Basket->dirt.total_mass());
    return out;
}

FWroughtBasket& UWroughtSimSubsystem::GetBasket()
{
    if (!Basket) Basket = MakePimpl<FWroughtBasket>();
    return *Basket;
}

FWroughtVatState UWroughtSimSubsystem::RenderVat(bool bJustMade) const
{
    using namespace wrought;
    FWroughtVatState out;
    if (!Basket) { out.bHasWashPan = bHasWashPan; return out; }

    const VatGame& vat = Basket->vat;
    out.bStirred      = vat.stirred();
    out.Stage         = vat.stage_now();
    out.Cloudiness    = static_cast<float>(vat.cloudiness());
    out.ChargeKg      = static_cast<float>(vat.charge_kg());
    out.RoomKg        = static_cast<float>(vat.charge_room_kg());
    out.SettleSeconds = static_cast<float>(vat.settle_seconds());
    double pm = 0.0, pc = 0.0;
    vat.pour_preview(pm, pc);
    out.PreviewMassG  = static_cast<float>(pm * 1000.0);
    out.PreviewClayG  = static_cast<float>(pc * 1000.0);
    out.ClayBankedKg  = static_cast<float>(Basket->clay.total_mass());
    out.KaoliniteKg   = static_cast<float>(Basket->clay.phase_mass(KAOLINITE));
    out.DirtCarriedKg = static_cast<float>(Basket->dirt.total_mass());
    out.bHasWashPan   = bHasWashPan;
    out.bWashPanJustMade = bJustMade;
    return out;
}

void UWroughtSimSubsystem::TryMakeWashPan()
{
    using namespace wrought;
    if (bHasWashPan || !Basket) return;
    const double kao = Basket->clay.phase_mass(KAOLINITE);
    if (kao < WashPanClayKg) return;

    // Throw the pan from the heap AS IT IS: consume heap mass carrying WashPanClayKg
    // of kaolinite; the fine quartz that rides along goes in as temper (which is what
    // temper is). v1 collapses pinch/dry/fire into this moment -- the fire station
    // will un-collapse it. Scale the heap down proportionally; mass is conserved
    // into the pot, not destroyed.
    const double heap = Basket->clay.total_mass();
    const double consume = heap * (WashPanClayKg / kao);
    const double f = (heap - consume) / heap;
    for (int p = 0; p < N_PHASE; ++p)
        for (int s = 0; s < N_SIZE; ++s)
        {
            Basket->clay.freegrain[p][s] *= f;
            Basket->clay.composite[p][s] *= f;
        }
    bHasWashPan = true;
    StationFeedback.Add(TEXT("There is enough. You pinch the clay up into a wide, shallow dish and set it to dry."));
    StationFeedback.Add(TEXT("You have a pan. It is a bad one. It is yours."));
}

FWroughtVatState UWroughtSimSubsystem::VatFill()
{
    using namespace wrought;
    FWroughtBasket& b = GetBasket();
    const double before = b.dirt.total_mass();
    const double took = b.vat.fill(b.dirt);
    if (took > 0.0 && before > 0.0)
    {
        // The vat accepted `took` kg of the basket's mixture; remove exactly that,
        // same proportions, from the carried dirt.
        const double f = (before - took) / before;
        for (int p = 0; p < N_PHASE; ++p)
            for (int s = 0; s < N_SIZE; ++s)
            {
                b.dirt.freegrain[p][s] *= f;
                b.dirt.composite[p][s] *= f;
            }
    }
    return RenderVat();
}

FWroughtVatState UWroughtSimSubsystem::VatStir()
{
    GetBasket().vat.stir();
    return RenderVat();
}

FWroughtVatState UWroughtSimSubsystem::VatPour()
{
    using namespace wrought;
    FWroughtBasket& b = GetBasket();
    const Substance liquor = b.vat.pour();
    const bool hadPan = bHasWashPan;
    if (liquor.total_mass() > 0.0)
    {
        AddInto(b.clay, liquor);
        TryMakeWashPan();
    }
    return RenderVat(!hadPan && bHasWashPan);
}

FWroughtVatState UWroughtSimSubsystem::VatDump()
{
    GetBasket().vat.dump();
    return RenderVat();
}

FWroughtVatState UWroughtSimSubsystem::VatAdvance(float DeltaSeconds)
{
    using namespace wrought;
    FWroughtBasket& b = GetBasket();
    // Fixed-step underneath, like the pan host: accumulate real seconds, take whole
    // VAT_DT steps, never scale one step by a frame's dt. Guard a huge hitch.
    VatAccumulator += FMath::Max(0.f, DeltaSeconds);
    const float MaxCatchUp = static_cast<float>(VAT_DT) * 40.f;
    if (VatAccumulator > MaxCatchUp) VatAccumulator = MaxCatchUp;
    while (VatAccumulator >= static_cast<float>(VAT_DT))
    {
        b.vat.tick();
        VatAccumulator -= static_cast<float>(VAT_DT);
    }
    return RenderVat();
}

FWroughtVatState UWroughtSimSubsystem::VatState() const
{
    return RenderVat();
}

TArray<FString> UWroughtSimSubsystem::VatDrainFeedback()
{
    TArray<FString> out;
    if (Basket)
        for (const std::string& l : Basket->vat.drain_feedback())
            out.Add(FString(UTF8_TO_TCHAR(l.c_str())));
    out.Append(StationFeedback);
    StationFeedback.Reset();
    return out;
}

float UWroughtSimSubsystem::CarriedKaoliniteKg() const
{
    return Basket ? static_cast<float>(Basket->clay.phase_mass(wrought::KAOLINITE)) : 0.f;
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
