using UnrealBuildTool;
using System.Collections.Generic;

// The dedicated-server target — THIS is what cooks into the Linux binary the LAN pod
// runs. It has no client rendering; it stands the wrought world up and serves it.
//
// Cook it headless on a machine with the engine installed (NOT the k3s cluster node):
//
//   RunUAT BuildCookRun -project=<path>/Wrought.uproject \
//     -noP4 -platform=Linux -clientconfig=Development -serverconfig=Development \
//     -server -noclient -cook -stage -pak -archive -archivedirectory=<out>
//
// The archived WroughtServer/ (binary + cooked content) is what gets wrapped into
// <registry>/wrought-server:latest — the image the staged 'wrought' catalog
// entry (ue/deploy/wrought.catalog.yaml) deploys onto node-b.
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
