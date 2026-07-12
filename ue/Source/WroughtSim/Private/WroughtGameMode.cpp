#include "WroughtGameMode.h"

AWroughtGameMode::AWroughtGameMode()
{
	// No default pawn: a dedicated wrought host has no avatar to possess until a client
	// connects, and there is no client yet. When one exists, DefaultPawnClass and a
	// PlayerController that forwards crosshair hits into WroughtSimSubsystem::BiteAt go
	// here. Seamless travel keeps map changes clean once there is more than one map.
	bUseSeamlessTravel = true;
}
