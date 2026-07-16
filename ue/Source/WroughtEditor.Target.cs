using UnrealBuildTool;
using System.Collections.Generic;

// The editor target — needed to open the project in UnrealEditor (to author the boot
// map and run the Wrought.Seam automation test from the Session Frontend).
public class WroughtEditorTarget : TargetRules
{
	public WroughtEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("WroughtSim");
		ExtraModuleNames.Add("WroughtLandscape");   // editor-only landscape authoring (headless valley sculpt)
	}
}
