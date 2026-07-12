#pragma once

// The authoritative host for a wrought world. A dedicated server boots into a map
// running this game mode; the WroughtSimSubsystem loads alongside it (a WorldSubsystem
// is created for every world, including a headless server's) and owns the sim.
//
// This base is deliberately NETWORK-NEUTRAL: it stands the world up and serves it,
// nothing more. Whether wrought grows into replicated multiplayer (clients driving
// BiteAt over the wire, replicated FWroughtBite results) or stays a single authoritative
// host for one future client is the open design fork -- and it does not change what this
// class needs to be in order to cook. Replication is added on top of this, not woven in.

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WroughtGameMode.generated.h"

UCLASS()
class WROUGHTSIM_API AWroughtGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWroughtGameMode();
};
