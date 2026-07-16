#pragma once

// The registration seam, Unreal side. This header stays PURE Unreal on purpose: no
// wrought type appears here, so Unreal Header Tool never parses the sim's namespace.
// Every #include of core/ lives in the .cpp. The whole spatial contract is two lines
// (WorldToPlace) -- geology.h never sees the world, only Place{x,y} in meters.

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/PimplPtr.h"
#include "WroughtSimSubsystem.generated.h"

// The carried stock — the basket on the player's back. Its DEFINITION lives in the
// .cpp because it holds real wrought::Substance grids, and this header stays pure
// Unreal (no wrought type ever named here, so UHT never parses the namespace). This
// is a plain Unreal-named PIMPL forward declaration, not a sim type.
struct FWroughtBasket;

// One phase's line on the assay panel: what came up, and in what state. FREE mass is
// pan-ready at the face; LOCKED mass is still composite rock the breaker must crush
// (geology.h: hard-rock sulfide rides up locked, a placer comes up free).
USTRUCT(BlueprintType)
struct FWroughtPhaseReadout
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Wrought") FString Phase;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float Grade = 0.f;        // mass fraction of the bite
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float FreeMassKg = 0.f;   // liberated at the face
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float LockedMassKg = 0.f; // composite -- needs the breaker
};

// The whole result of one bite, rendered for the panel. bWonRock is false when the
// tool skipped off the rock (win_bite returned nothing) -- that IS the pick gate.
USTRUCT(BlueprintType)
struct FWroughtBite
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bWonRock = false;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float TotalMassKg = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") FVector2D Place = FVector2D::ZeroVector; // meters
    UPROPERTY(BlueprintReadOnly, Category="Wrought") TArray<FWroughtPhaseReadout> Phases;
};

// The readout of one levigation pass, rendered for the belt. The clay you poured off
// (liquor) and the tailings you left (sediment), plus the running basket totals. This
// is the assay-row shape's cousin for a separation the player performs, not a bite.
USTRUCT(BlueprintType)
struct FWroughtDecant
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bDecanted = false;      // false when the basket held no dirt to pour
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float ClayKg = 0.f;          // liquor poured off THIS pass
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float ClayGrade = 0.f;       // kaolinite fraction of that liquor (0..1)
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float TailingsKg = 0.f;      // sediment left behind as dirt this pass
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float ClayTotalKg = 0.f;     // clay now in the basket (running)
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float DirtRemainingKg = 0.f; // dirt still in the basket (running)
};

// The live state of the vat station -- the interactive levigation session (core
// vat_game.h) rendered for the HUD. The decision aid is the pour preview: what a
// pour RIGHT NOW would carry out, and how much of it is clay. Stage narrates the
// water (0 fresh stir .. 3 clay haze); Cloudiness drives a material/visual.
USTRUCT(BlueprintType)
struct FWroughtVatState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bStirred = false;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") int32 Stage = 0;            // 0..3
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float Cloudiness = 0.f;     // 0..1
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float ChargeKg = 0.f;       // mud in the hollow
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float RoomKg = 0.f;         // fill space left
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float SettleSeconds = 0.f;  // true sim seconds
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float PreviewMassG = 0.f;   // a pour now: solids over the lip
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float PreviewClayG = 0.f;   // ... of which clay
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float ClayBankedKg = 0.f;   // basket clay heap, total
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float KaoliniteKg = 0.f;    // ... of which true kaolinite
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float DirtCarriedKg = 0.f;  // basket dirt heap
    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bHasWashPan = false;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bWashPanJustMade = false; // true on the pour that earned it
};

// A WorldSubsystem so it is auto-instantiated per world and reachable from any
// Blueprint. It holds the small amount of state the player drives (where the plane's
// origin is, which depth tier and tool are selected) and exposes the core round-trip.
UCLASS()
class WROUGHTSIM_API UWroughtSimSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // --- The registration frame. World cm in, Place meters out. ---
    // This is the entire spatial adapter. Unreal is centimeters; the sim is meters
    // from an arbitrary origin. Set PlaneOrigin to the world location you want to be
    // Place (0,0) -- the copper hill sits at Place (0,0), so origin at the hill's foot
    // lines the DEPOSITS[] frame up with the Landscape for free.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wrought")
    FVector PlaneOrigin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wrought")
    float UnitsPerMeter = 100.f;   // Unreal cm per sim meter

    // --- State the player drives. ---
    // 0..N_TIER-1 == SURFACE, MIDDLE, DEEP. The dig bar crosses tiers; there is no
    // z-coordinate (geology.h: depth is a discrete tier, not a carved axis).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wrought", meta=(ClampMin="0", ClampMax="2"))
    int32 SelectedTier = 0;

    // Equipped tool's blow energy, on haft.h's arbitrary scale. A bare hand clears the
    // weathered cap and a placer but skips the deep rock; a pick clears every tier.
    // Set it with EquipBareHand()/EquipSaplingPick(), or from a real Hafted::bite().
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wrought")
    float BlowEnergy = 0.f;

    // Bite mass per tier, kg -- the tool's REACH (a pick wins ~10x a hand's read of the
    // very same makeup). Placeholder; it is a slice-local pacing knob, not a sim number.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wrought")
    float BiteMassKg = 1.f;

    // Convert a world hit (e.g. a line trace onto the Landscape) to a Place. Pure.
    UFUNCTION(BlueprintPure, Category="Wrought")
    FVector2D WorldToPlace(FVector WorldLocation) const;

    // One bite at the selected tier with the equipped tool -- the core round-trip the
    // smoke-test exists to prove. Click ground -> this -> panel.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    FWroughtBite BiteAt(FVector WorldLocation);

    // A full-depth dig at one spot, tier by tier, gated on the tool's blow energy: a
    // bare hand brings up only the oxide cap, a pick brings the mixed oxide+sulfide column.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    FWroughtBite DigColumnAt(FVector WorldLocation);

    // Convenience tool presets so a Blueprint can equip without building the whole
    // knap->haft chain in the editor. These call the real haft.h math -- the numbers
    // are the model's own, not hardcoded magnitudes (head mass / haft length are the
    // only placeholders, and they are player-equipment choices, not sim constants).
    UFUNCTION(BlueprintCallable, Category="Wrought")
    void EquipBareHand();

    UFUNCTION(BlueprintCallable, Category="Wrought")
    void EquipSaplingPick();

    // --- The basket. Carried stock is REAL Substance, held server-side. ---
    // Every winning bite pours its full grain composition into the basket's dirt heap
    // (BiteAt/DigColumnAt do this), not just a scalar mass. The belt only ever reads
    // the kg getters; the grid stays in the sim, which is the only thing that can
    // separate it.
    UFUNCTION(BlueprintPure, Category="Wrought")
    float CarriedDirtKg() const;

    UFUNCTION(BlueprintPure, Category="Wrought")
    float CarriedClayKg() const;

    // Levigation — the second station. Stir the carried dirt into the authored HOLLOW
    // vessel, wait `Seconds`, pour off the clear water above the sediment. The liquor
    // IS clay: it accumulates in the basket's clay heap; the sediment stays as dirt.
    // Patience buys grade, not recovery (levigate.h). Runs the core decant(); the sim
    // is the process, this only hands it the basket and renders the result.
    // SUPERSEDED by the interactive vat below for play; kept for tests/tools.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    FWroughtDecant Levigate(float Seconds);

    // --- The vat — the FIRST GATE, played. (core vat_game.h; the physics is
    // levigate.h's decant, the tempo is the compression law's.) The loop:
    // gather at the clay bank -> VatFill from the basket -> VatStir -> the water
    // clears while VatAdvance ticks -> VatPour when YOU judge it -> clay banks ->
    // enough kaolinite and the wash pan pops into the tool belt. The pan is
    // EARNED by separation, never granted by proximity. ---

    // Tip the carried dirt into the hollow (as much as it has room for).
    UFUNCTION(BlueprintCallable, Category="Wrought") FWroughtVatState VatFill();
    UFUNCTION(BlueprintCallable, Category="Wrought") FWroughtVatState VatStir();
    UFUNCTION(BlueprintCallable, Category="Wrought") FWroughtVatState VatPour();
    UFUNCTION(BlueprintCallable, Category="Wrought") FWroughtVatState VatDump();

    // Advance the settle by real elapsed seconds (fixed-step underneath; call from
    // a Tick while the player is at the station). Returns the live state.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    FWroughtVatState VatAdvance(float DeltaSeconds);

    UFUNCTION(BlueprintPure, Category="Wrought") FWroughtVatState VatState() const;

    // Chat lines the vat produced since last drained (fill/stir/pour narration,
    // the water's stage changes, the pan-made line). Bind to the chat panel.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    TArray<FString> VatDrainFeedback();

    // True kaolinite in the clay heap -- the mass that gates the pan. The heap's
    // total (CarriedClayKg) also carries fine quartz that rode the pour; it goes
    // into the pot as temper, but it is not what makes the pot possible.
    UFUNCTION(BlueprintPure, Category="Wrought") float CarriedKaoliniteKg() const;

    UFUNCTION(BlueprintPure, Category="Wrought") bool HasWashPan() const { return bHasWashPan; }

    // Kaolinite that must be banked before the first pan pops into the belt.
    // AUTHORED, UNVERIFIED (a pinch-pot pan's wet mass, roughly); v1 collapses
    // pinch/dry/fire into this threshold -- the fire station un-collapses it later.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wrought")
    float WashPanClayKg = 2.f;

private:
    // The basket. Definition (two wrought::Substance heaps + the vat session) is in
    // the .cpp — see the forward declaration above. TPimplPtr gives an out-of-line
    // deleter for the incomplete type, so the header never needs the sim's definition.
    TPimplPtr<FWroughtBasket> Basket;

    // Earned state: set the moment the clay heap crosses WashPanClayKg on a pour.
    // Persistent for the session; save-game persistence is the world's job later.
    UPROPERTY(VisibleAnywhere, Category="Wrought")
    bool bHasWashPan = false;

    float VatAccumulator = 0.f;         // leftover real seconds not yet stepped
    TArray<FString> StationFeedback;    // station-level lines (e.g. the pan-made line)

    FWroughtBasket& GetBasket();        // lazily creates the pimpl
    FWroughtVatState RenderVat(bool bJustMade = false) const;
    void TryMakeWashPan();              // consumes clay, sets bHasWashPan, says so
};
