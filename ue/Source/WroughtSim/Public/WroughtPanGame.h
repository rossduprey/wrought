#pragma once

// The pan minigame, on the UE side. Same seam discipline as WroughtSimSubsystem: this
// header stays PURE Unreal -- no wrought type appears here, so Unreal Header Tool never
// parses the sim's namespace. The core (core/pan_game.h) is included only in the .cpp,
// held behind a forward-declared pointer.
//
// The pan is a client-side, single-player interaction -- you kneel at a river. Unlike
// the bite seam it needs no authority/replication for v1: the pawn owns one of these,
// drives it from Tick, and binds its feedback to the on-screen chat panel.
//
//   pawn Tick --Advance(dt)--> [core steps at fixed PAN_DT] --> OnPanFeedback(line) per
//     emitted text line (nag/ambient/endgame) --> HUD chat panel appends it;
//     OnPanState(state) once per frame --> HUD paints the live pan/assay.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WroughtPanGame.generated.h"

// One row of the live assay panel: grade and recovery side by side, per phase.
USTRUCT(BlueprintType)
struct FWroughtPanRow
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Wrought") FString Phase;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float Grams = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float Grade = 0.f;  // % of the pan
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float Kept = 0.f;   // % of the ground (recovery)
    UPROPERTY(BlueprintReadOnly, Category="Wrought") FString Arrow;      // "^" rising, "v" falling, "" flat
    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bGone = false; // washed out of the pan
};

// The whole live state of the pan, for the HUD's pan visual and assay.
USTRUCT(BlueprintType)
struct FWroughtPanState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bRunning = false;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float TimeSeconds = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float CutFraction = 0.f;   // 0..1 of MAX_CUT
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float PanMassGrams = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float BlackFraction = 0.f; // 0..1
    UPROPERTY(BlueprintReadOnly, Category="Wrought") FString Colour;            // "pale".."black, and it glitters"
    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bHasStones = false;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") float TubMassGrams = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") bool bTubGlitters = false;
    UPROPERTY(BlueprintReadOnly, Category="Wrought") TArray<FWroughtPanRow> Rows;
};

// A line of feedback text the pan produced (nag, ambient, or an endgame report line).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWroughtPanFeedback, const FString&, Line);
// The live pan state, broadcast once per frame while panning.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWroughtPanStateChanged, const FWroughtPanState&, State);

// wrought::PanGame lives in core/pan_game.h; the .cpp holds it behind this pointer.
namespace wrought { class PanGame; }

UCLASS(BlueprintType, Blueprintable)
class WROUGHTSIM_API UWroughtPanGame : public UObject
{
    GENERATED_BODY()

public:
    // Begin a session on a deposit: 0 river sand, 1 black-sand bar, 2 weathered outcrop.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    void StartPan(int32 Deposit = 0);

    // The verbs. Swirl is the sustained motion -- call it repeatedly to hold the water
    // moving (an input axis/hold, or a rapid tap).
    UFUNCTION(BlueprintCallable, Category="Wrought") void Swirl();
    UFUNCTION(BlueprintCallable, Category="Wrought") void PickStone();
    UFUNCTION(BlueprintCallable, Category="Wrought") void RepanTub();
    UFUNCTION(BlueprintCallable, Category="Wrought") void NewDeposit();
    UFUNCTION(BlueprintCallable, Category="Wrought") void Keep();

    // Advance the sim by real elapsed seconds. The core is fixed-step (PAN_DT), so this
    // accumulates and steps floor(elapsed/PAN_DT) times, broadcasting feedback + state.
    // Call from the pawn's Tick while a session is running.
    UFUNCTION(BlueprintCallable, Category="Wrought")
    void Advance(float DeltaSeconds);

    UFUNCTION(BlueprintPure, Category="Wrought") bool IsPanning() const;
    UFUNCTION(BlueprintPure, Category="Wrought") FWroughtPanState GetState() const;

    // Bound by the HUD: OnPanFeedback -> chat panel; OnPanState -> pan visual/assay.
    UPROPERTY(BlueprintAssignable, Category="Wrought") FWroughtPanFeedback OnPanFeedback;
    UPROPERTY(BlueprintAssignable, Category="Wrought") FWroughtPanStateChanged OnPanState;

    virtual void BeginDestroy() override;

private:
    void PumpAndBroadcast();

    wrought::PanGame* Game = nullptr;   // owned; freed in BeginDestroy
    float Accumulator = 0.f;            // leftover real seconds not yet stepped
};
