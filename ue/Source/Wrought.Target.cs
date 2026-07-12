using UnrealBuildTool;
using System.Collections.Generic;

// The default target — a standalone/client Wrought build. The dedicated server the LAN
// pod runs is WroughtServer.Target.cs; this one exists so the project is complete and
// so a developer can run the game with a listen server locally.
public class WroughtTarget : TargetRules
{
	public WroughtTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("WroughtSim");
	}
}
