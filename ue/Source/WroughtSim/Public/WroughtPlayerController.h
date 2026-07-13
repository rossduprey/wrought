#pragma once

// The networked seam. wrought runs authoritative on the dedicated server: the sim (the
// WroughtSimSubsystem) only ever executes there. A client never touches geology.h -- it
// sends the one thing it knows (where its crosshair hit the ground) up to the server, and
// receives back the one thing it's allowed to see (the assay that came up). Same
// registration seam as the single-host adapter, now stretched across the wire:
//
//   client crosshair hit --RequestBite--> [ServerRequestBite RPC]
//     --> server: WroughtSimSubsystem::BiteAt --> [ClientReceiveBite RPC]
//       --> client: OnBiteReceived fires, HUD reads the panel.
//
// The hole, the ground, the deposits -- none of that replicates. Only Place-in / bite-out
// crosses, exactly the seam the header-only sim was built to expose.

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WroughtSimSubsystem.h"   // FWroughtBite
#include "WroughtPlayerController.generated.h"

// Fired on the client when a requested bite's result lands. The HUD/panel binds this.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWroughtBiteReceived, FWroughtBite, Bite);

UCLASS()
class WROUGHTSIM_API AWroughtPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Call locally on the owning client (from a crosshair line-trace). Forwards to the
	// server; the client never runs the sim itself.
	UFUNCTION(BlueprintCallable, Category="Wrought")
	void RequestBite(FVector WorldHit);

	// Runs on the server -- the only place BiteAt is ever called. Reliable so a bite is
	// never silently dropped.
	UFUNCTION(Server, Reliable)
	void ServerRequestBite(FVector WorldHit);

	// Server -> the requesting client only: the assay to show. Reliable.
	UFUNCTION(Client, Reliable)
	void ClientReceiveBite(FWroughtBite Bite);

	// Bound by the client's HUD; broadcast when ClientReceiveBite arrives.
	UPROPERTY(BlueprintAssignable, Category="Wrought")
	FWroughtBiteReceived OnBiteReceived;
};
