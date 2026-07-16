#include "WroughtSimSubsystem.h"

#include "EngineUtils.h"   // TActorIterator, for the TreeStand proximity scan

// The wrought sim. Header-only C++17, all of it confined to this .cpp so Unreal Header
// Tool never sees the namespace. geology.h pulls substance/settling; haft.h pulls
// knap.h -- these two umbrellas cover the whole seam.
#include "geology.h"
#include "haft.h"
#include "levigate.h"   // decant(), HOLLOW, DecantResult -- the second separator
#include "vat_game.h"   // the levigation session, played -- the first gate
#include "firekit.h"    // pulls in fuel.h: Wood, make_smolder -- the earned spark

#include <cmath>
#include <map>
#include <utility>

// One tree's reachable stock -- gather.cpp's clearing, hung on a single TreeStand.
// Full values are that slice's authored pacing (DEADWOOD_FULL / LITTER_FULL); each
// tree thins independently as it is worked, and the standing timber never appears
// here at all -- hands cannot fill that column, which is the whole finding.
struct FWroughtStand
{
    double deadwood = 3.0;   // kg of dead sticks within a hand's reach (gather.cpp DEADWOOD_FULL)
    double litter   = 0.5;   // kg of dry needle litter under it (gather.cpp LITTER_FULL)
};

// The basket's real contents: Substance heaps, the wood stock, the vat session, and
// the worked-stand map -- the sim's own types. Defined here (not the header) so no
// wrought type crosses into the reflected surface. TPimplPtr in the header holds one
// of these; the belt sees only the getters.
struct FWroughtBasket
{
    wrought::Substance dirt;   // raw dug stock, poured in bite by bite
    wrought::Substance clay;   // levigated liquor, accumulated pass by pass
    wrought::VatGame   vat;    // the hollow scraped in the bank (world station state)
    wrought::Wood      wood;   // gathered dead wood: tinder + sticks; timber stays 0 by hand
    std::map<std::pair<int,int>, FWroughtStand> stands; // per-tree stock, keyed by rounded Place m
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

// ---------------------------------------------------------------------------
// The tree station. gather.cpp's clearing, one armful per arrival.

namespace
{
    // The compression law, applied to hands: one walk-up stands in for minutes of
    // raking and pulling, so an armful is a fixed slice of the stand's REMAINING
    // reachable share -- the taper gather.cpp plays in real time, compressed to the
    // visit. Below PICKED_CLEAN the easy pickings are spent and the tree gives
    // nothing; you do not empty a stand, you exhaust what hands can take from it.
    // All AUTHORED pacing, the counterpart of gather.cpp's PULL_RATE/RAKE_RATE.
    constexpr double ARMFUL_STICKS_KG  = 0.6;    // per visit at a full stand
    constexpr double ARMFUL_NEEDLES_KG = 0.12;
    constexpr double STAND_DEADWOOD_FULL = 3.0;  // == gather.cpp DEADWOOD_FULL
    constexpr double STAND_LITTER_FULL   = 0.5;  // == gather.cpp LITTER_FULL
    constexpr double PICKED_CLEAN = 0.05;        // == gather.cpp PICKED_CLEAN
    constexpr double GATHER_MOISTURE = 0.15;     // == gather.cpp: dead standing wood is dry-ish
}

FWroughtGatherState UWroughtSimSubsystem::RenderGather(bool bJustMade) const
{
    FWroughtGatherState out;
    out.bHasSmolderKit = bHasSmolderKit;
    out.bSmolderKitJustMade = bJustMade;
    if (!Basket) { out.WoodRoomKg = BasketWoodCapacityKg; return out; }
    out.SticksCarriedKg = static_cast<float>(Basket->wood.sticks);
    out.TinderCarriedKg = static_cast<float>(Basket->wood.tinder);
    out.WoodRoomKg = FMath::Max(0.f, BasketWoodCapacityKg
        - static_cast<float>(Basket->wood.sticks + Basket->wood.tinder));
    return out;
}

bool UWroughtSimSubsystem::TryMakeSmolderKit(TArray<FString>& OutLines)
{
    using namespace wrought;
    if (bHasSmolderKit || !Basket) return false;
    const double need = SmolderKitSticksKg;
    if (Basket->wood.sticks < need) return false;

    // The real gate: make_smolder() reads the stock's own dryness against fuel.h's
    // TINDER_MOISTURE_MAX. Green wood takes no ember, however much of it you carry.
    const SmolderKit kit = make_smolder(Basket->wood);
    if (!kit.made) return false;

    // Carve it: the set costs its stock, like the pan cost its clay. Possession is
    // the whole of the tool from here -- it does not wear (firekit.h).
    Basket->wood.sticks -= need;
    bHasSmolderKit = true;
    OutLines.Add(TEXT("There is enough dry wood. You carve a hearth board and a spindle, and work a bow from a bent stick."));
    OutLines.Add(TEXT("You have a smolder kit. Every fire you ever light starts here."));
    return true;
}

FWroughtGatherState UWroughtSimSubsystem::GatherAtTree(FVector TreeWorldLocation)
{
    using namespace wrought;
    FWroughtBasket& b = GetBasket();

    // Key the stand by its rounded Place -- one stock per placed tree.
    const FVector2D pl = WorldToPlace(TreeWorldLocation);
    const std::pair<int,int> key{ (int)std::lround(pl.X), (int)std::lround(pl.Y) };
    FWroughtStand& st = b.stands.try_emplace(key).first->second;

    const double dwFrac = st.deadwood / STAND_DEADWOOD_FULL;
    const double ltFrac = st.litter / STAND_LITTER_FULL;

    TArray<FString> lines;
    double room = FMath::Max(0.f, BasketWoodCapacityKg)
                - (b.wood.sticks + b.wood.tinder);

    double gotSticks = 0.0, gotNeedles = 0.0;
    const bool spent = dwFrac <= PICKED_CLEAN && ltFrac <= PICKED_CLEAN;
    if (spent)
    {
        lines.Add(TEXT("This tree is picked clean -- the deadwood in reach is stripped and the litter raked to bare ground."));
    }
    else if (room <= 1e-6)
    {
        lines.Add(TEXT("Your basket is full. What you carry is what you can use -- go spend some of it."));
    }
    else
    {
        // The armful: sticks first (the fuel), then needles (the match), each a slice
        // of THIS tree's remaining reach, together capped by the basket's room.
        if (dwFrac > PICKED_CLEAN)
        {
            gotSticks = FMath::Min3(ARMFUL_STICKS_KG * dwFrac, st.deadwood, room);
            st.deadwood -= gotSticks;
            room        -= gotSticks;
        }
        if (ltFrac > PICKED_CLEAN && room > 1e-6)
        {
            gotNeedles = FMath::Min3(ARMFUL_NEEDLES_KG * ltFrac, st.litter, room);
            st.litter -= gotNeedles;
            room      -= gotNeedles;
        }

        // Everything hands win here is DEAD and dry-ish -- that is why you take it.
        // The timber column stays zero: the felling wall, felt, not stated.
        b.wood.sticks  += gotSticks;
        b.wood.tinder  += gotNeedles;
        b.wood.moisture = GATHER_MOISTURE;

        FString give;
        if (gotSticks > 1e-6)
            give = FString::Printf(TEXT("You snap %.0f g of dead branches from the trunk"), gotSticks * 1000.0);
        if (gotNeedles > 1e-6)
            give += FString::Printf(TEXT("%s %.0f g of dry needles raked from under it"),
                give.IsEmpty() ? TEXT("You rake up") : TEXT(" and pocket"), gotNeedles * 1000.0);
        if (!give.IsEmpty())
            lines.Add(give + TEXT("."));
        if (st.deadwood / STAND_DEADWOOD_FULL <= PICKED_CLEAN && gotSticks > 1e-6)
            lines.Add(TEXT("The easy deadwood here is stripped; what is left is green or too high."));
        if (room <= 1e-6)
            lines.Add(TEXT("The basket is packed to the brim."));
    }

    const bool justMade = TryMakeSmolderKit(lines);

    FWroughtGatherState out = RenderGather(justMade);
    out.bGave = gotSticks + gotNeedles > 1e-6;
    out.SticksGivenG  = static_cast<float>(gotSticks * 1000.0);
    out.NeedlesGivenG = static_cast<float>(gotNeedles * 1000.0);
    out.TreeDeadwoodFrac = static_cast<float>(st.deadwood / STAND_DEADWOOD_FULL);
    out.TreeLitterFrac   = static_cast<float>(st.litter / STAND_LITTER_FULL);
    out.bTreeSpent = out.TreeDeadwoodFrac <= PICKED_CLEAN && out.TreeLitterFrac <= PICKED_CLEAN;
    out.Lines = MoveTemp(lines);
    out.bAtTree = true;
    out.Readout = FString::Printf(
        TEXT("gave %.0f g sticks + %.0f g needles | carrying %.2f kg sticks, %.0f g tinder | room %.1f kg%s"),
        out.SticksGivenG, out.NeedlesGivenG,
        out.SticksCarriedKg, out.TinderCarriedKg * 1000.f, out.WoodRoomKg,
        out.bTreeSpent ? TEXT(" | picked clean") : TEXT(""));
    return out;
}

FWroughtGatherState UWroughtSimSubsystem::GatherState() const
{
    return RenderGather();
}

FWroughtGatherState UWroughtSimSubsystem::GatherProximityTick(FVector PawnLocation)
{
    // The station scan: nearest TreeStand-tagged actor in reach. ~50 tagged trees;
    // a flat iterate per tick is nothing, and never goes stale on a re-scatter.
    AActor* Nearest = nullptr;
    float BestSq = TreeStationRangeCm * TreeStationRangeCm;
    if (UWorld* World = GetWorld())
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->ActorHasTag(FName(TEXT("TreeStand")))) continue;
            const float DSq = FVector::DistSquared(It->GetActorLocation(), PawnLocation);
            if (DSq < BestSq) { BestSq = DSq; Nearest = *It; }
        }

    if (!Nearest)
    {
        // Left the stand: drop the latch so the NEXT walk-up gathers again.
        CurrentTree.Reset();
        return RenderGather();   // bAtTree stays false
    }

    if (CurrentTree.Get() != Nearest)
    {
        // Arrival IS the verb: latch this tree and take the armful, once.
        CurrentTree = Nearest;
        return GatherAtTree(Nearest->GetActorLocation());
    }

    // Still standing at the latched tree: live readout, no new armful, no chat.
    FWroughtGatherState out = RenderGather();
    out.bAtTree = true;
    const FVector2D pl = WorldToPlace(Nearest->GetActorLocation());
    if (Basket)
    {
        const std::pair<int,int> key{ (int)std::lround(pl.X), (int)std::lround(pl.Y) };
        const auto found = Basket->stands.find(key);
        if (found != Basket->stands.end())
        {
            out.TreeDeadwoodFrac = static_cast<float>(found->second.deadwood / STAND_DEADWOOD_FULL);
            out.TreeLitterFrac   = static_cast<float>(found->second.litter / STAND_LITTER_FULL);
            out.bTreeSpent = out.TreeDeadwoodFrac <= PICKED_CLEAN && out.TreeLitterFrac <= PICKED_CLEAN;
        }
    }
    out.Readout = FString::Printf(
        TEXT("carrying %.2f kg sticks, %.0f g tinder | room %.1f kg%s"),
        out.SticksCarriedKg, out.TinderCarriedKg * 1000.f, out.WoodRoomKg,
        out.bTreeSpent ? TEXT(" | picked clean") : TEXT(""));
    return out;
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
