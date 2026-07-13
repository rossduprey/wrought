#include "WroughtPlayerController.h"

#include "Engine/World.h"

void AWroughtPlayerController::RequestBite(FVector WorldHit)
{
	// Client side: don't run the sim, just ask the server. (If this is ever called on a
	// listen-server host, the RPC still routes correctly to the authority.)
	ServerRequestBite(WorldHit);
}

void AWroughtPlayerController::ServerRequestBite_Implementation(FVector WorldHit)
{
	// Authority only. The subsystem is created per-world, so it exists on the headless
	// server world too. This is the single point where a bite actually happens.
	UWorld* World = GetWorld();
	UWroughtSimSubsystem* Sim = World ? World->GetSubsystem<UWroughtSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}

	const FWroughtBite Bite = Sim->BiteAt(WorldHit);
	ClientReceiveBite(Bite);
}

void AWroughtPlayerController::ClientReceiveBite_Implementation(FWroughtBite Bite)
{
	// Back on the requesting client: hand the assay to whatever the HUD bound.
	OnBiteReceived.Broadcast(Bite);
}
