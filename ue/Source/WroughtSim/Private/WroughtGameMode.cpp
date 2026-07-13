#include "WroughtGameMode.h"

#include "WroughtPlayerController.h"

AWroughtGameMode::AWroughtGameMode()
{
	// Clients connect through the wrought controller: it forwards crosshair hits to the
	// server and receives the assay back (WroughtPlayerController.h has the whole seam).
	PlayerControllerClass = AWroughtPlayerController::StaticClass();

	// No default pawn yet: wrought players drive a crosshair and read an assay panel, not
	// an avatar. A spectator-style pawn (or none) is right until there's a body to give
	// them. Seamless travel keeps map changes clean once there's more than one map.
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = true;
}
