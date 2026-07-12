using UnrealBuildTool;
using System.IO;

// Build rules for the wrought<->Unreal seam. There is no library to build: the sim
// under core/ is header-only C++17, so this module only needs the core/ dir on its
// include path and it compiles straight into the game. Prove THIS builds clean in
// UBT and the "the sim is portable" claim is proven for real, not asserted (STATUS.md,
// the #28 seam smoke-test).
public class WroughtSim : ModuleRules
{
    public WroughtSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp17;   // core/ is C++17; do not silently upgrade it

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

        // Where the wrought sim headers live. Prefer an explicit checkout path via the
        // WROUGHT_CORE environment variable; otherwise assume this module sits at
        // <wrought>/ue/Source/WroughtSim/ and reach back up to <wrought>/core/.
        string CoreDir = System.Environment.GetEnvironmentVariable("WROUGHT_CORE");
        if (string.IsNullOrEmpty(CoreDir))
            CoreDir = Path.Combine(ModuleDirectory, "..", "..", "..", "core");
        PrivateIncludePaths.Add(Path.GetFullPath(CoreDir));
    }
}
