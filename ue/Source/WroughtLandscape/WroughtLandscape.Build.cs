using UnrealBuildTool;

// Editor-only authoring module: exposes a BlueprintCallable to build an ALandscape from a
// packed heightmap so the wrought valley can be sculpted HEADLESS over the MCP loop (UE's
// Landscape Mode UI is modal and dies over the SSH-driven editor; ALandscape::Import is
// plain C++, not a UFUNCTION, so a Python toolset can't reach it without this wrapper).
// Added ONLY to WroughtEditor.Target.cs — never cooks into the game/server targets.
public class WroughtLandscape : ModuleRules
{
	public WroughtLandscape(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Landscape",   // ALandscape::Import, ULandscapeInfo
			"UnrealEd",    // GEditor / editor world context
		});
	}
}
