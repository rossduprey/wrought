#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WroughtLandscapeLibrary.generated.h"

class ALandscape;

// Headless landscape authoring for wrought. The single entry point wraps
// ALandscape::Import so the MCP Python toolset (landscape.py) can sculpt the valley
// onto the sim's Place{x,y} frame without touching the modal Landscape Mode UI.
UCLASS()
class WROUGHTLANDSCAPE_API UWroughtLandscapeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Create a new ALandscape in the editor world from a base64-packed uint16 heightmap.
	//
	//   SizeX, SizeY        vertex counts. (SizeX-1) and (SizeY-1) must each be an exact
	//                       multiple of (NumSubsections * QuadsPerSection).
	//   QuadsPerSection     quads per subsection (7/15/31/63/127/255).
	//   NumSubsections      subsections per component (1 or 2).
	//   Location            world-space location of the landscape's (0,0) corner, in cm.
	//   Scale               actor scale; X/Y set cm-per-quad (100 => 1 m/quad), Z scales
	//                       height (world Z = (h-32768) * Scale.Z / 128 cm).
	//   HeightsB64          base64 of SizeX*SizeY little-endian uint16, row-major
	//                       (row y-major: index = y*SizeX + x). 32768 == flat.
	//
	// Returns the created ALandscape, or nullptr on failure (see the output log).
	UFUNCTION(BlueprintCallable, Category = "Wrought")
	static ALandscape* CreateLandscapeFromHeightmapB64(
		int32 SizeX,
		int32 SizeY,
		int32 QuadsPerSection,
		int32 NumSubsections,
		FVector Location,
		FVector Scale,
		const FString& HeightsB64);
};
