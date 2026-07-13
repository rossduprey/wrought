using UnrealBuildTool;
using System.Collections.Generic;

// The dedicated-server target — THIS is what cooks into the Linux binary that runs the
// server. It has no client rendering; it stands the wrought world up and serves it.
//
// Cook it headless on a machine with the engine installed (not a lightweight host/container
// node — the engine is ~100GB):
//
//   RunUAT BuildCookRun -project=<path>/Wrought.uproject \
//     -noP4 -platform=Linux -clientconfig=Development -serverconfig=Development \
//     -server -noclient -cook -stage -pak -archive -archivedirectory=<out>
//
// The archived WroughtServer/ (binary + cooked content) gets wrapped into a container by
// ue/deploy/Dockerfile and pushed to your registry. See ue/deploy/README.md.
public class WroughtServerTarget : TargetRules
{
	public WroughtServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("WroughtSim");
	}
}
