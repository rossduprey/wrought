#include "WroughtLandscapeLibrary.h"

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/Base64.h"

// Mirrors the engine's own "New Landscape" creation path
// (LandscapeEditorDetailCustomization_NewLandscape.cpp): spawn ALandscape, scale it,
// then ALandscape::Import with the height map keyed under the ZERO guid (the final/base
// edit layer), while the landscape's own guid is a fresh one passed as Import's first arg.
ALandscape* UWroughtLandscapeLibrary::CreateLandscapeFromHeightmapB64(
	int32 SizeX,
	int32 SizeY,
	int32 QuadsPerSection,
	int32 NumSubsections,
	FVector Location,
	FVector Scale,
	const FString& HeightsB64)
{
#if WITH_EDITOR
	if (SizeX < 2 || SizeY < 2 || QuadsPerSection < 1 || NumSubsections < 1)
	{
		UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: bad dimensions %dx%d q=%d subs=%d"),
			SizeX, SizeY, QuadsPerSection, NumSubsections);
		return nullptr;
	}

	const int32 ComponentQuads = NumSubsections * QuadsPerSection;
	if ((SizeX - 1) % ComponentQuads != 0 || (SizeY - 1) % ComponentQuads != 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("WroughtLandscape: (Size-1) must be a multiple of NumSubsections*QuadsPerSection=%d; got %dx%d"),
			ComponentQuads, SizeX, SizeY);
		return nullptr;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: no editor world"));
		return nullptr;
	}

	// Decode the packed heights. Bytes are little-endian uint16 (host is LE), so a straight
	// memcpy into the uint16 array is correct.
	TArray<uint8> Bytes;
	if (!FBase64::Decode(HeightsB64, Bytes))
	{
		UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: base64 decode failed"));
		return nullptr;
	}
	const int32 ExpectedBytes = SizeX * SizeY * 2;
	if (Bytes.Num() != ExpectedBytes)
	{
		UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: decoded %d bytes, expected %d (%dx%d)"),
			Bytes.Num(), ExpectedBytes, SizeX, SizeY);
		return nullptr;
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(SizeX * SizeY);
	FMemory::Memcpy(Heights.GetData(), Bytes.GetData(), ExpectedBytes);

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	if (!Landscape)
	{
		UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: SpawnActor<ALandscape> failed"));
		return nullptr;
	}
	Landscape->SetActorRelativeScale3D(Scale);

	// Lighting LOD heuristic, straight from the engine's New Landscape path.
	Landscape->StaticLightingLOD =
		FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

	// Height data (and an empty material-layer set) live under the ZERO guid = the base layer.
	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	Landscape->Import(
		FGuid::NewGuid(),
		0, 0, SizeX - 1, SizeY - 1,
		NumSubsections, QuadsPerSection,
		HeightDataPerLayers,
		TEXT(""),
		MaterialLayerDataPerLayers,
		ELandscapeImportAlphamapType::Additive,
		TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: Import produced no ULandscapeInfo"));
		return Landscape;
	}
	LandscapeInfo->UpdateLayerInfoMap(Landscape);

	Landscape->SetActorLabel(TEXT("WroughtValley"));
	UE_LOG(LogTemp, Display, TEXT("WroughtLandscape: created %dx%d landscape at (%s)"),
		SizeX, SizeY, *Location.ToString());
	return Landscape;
#else
	UE_LOG(LogTemp, Error, TEXT("WroughtLandscape: editor-only"));
	return nullptr;
#endif
}
