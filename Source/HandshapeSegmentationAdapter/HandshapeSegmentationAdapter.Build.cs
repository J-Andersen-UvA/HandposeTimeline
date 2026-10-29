using UnrealBuildTool;
using System.IO;

public class HandshapeSegmentationAdapter : ModuleRules
{
    public HandshapeSegmentationAdapter(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "AnimationBlueprintLibrary",
                "Core",
                "CoreUObject",
                "Engine"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Eigen",
                "UnrealEd"
            });

        string dependencyRoot = Path.Combine(
            PluginDirectory,
            "ThirdParty",
            "HandshapeSegmentation");
        string dependencyLibrary = Path.Combine(
            dependencyRoot,
            "lib",
            "Win64",
            "HandshapeSegmentationUpstream.lib");
        if (!File.Exists(dependencyLibrary))
        {
            throw new BuildException(
                "Build ThirdParty/HandshapeSegmentation before compiling HandposeTimeline. " +
                "See ThirdParty/HandshapeSegmentation/README.md.");
        }
        PublicIncludePaths.Add(Path.Combine(dependencyRoot, "include"));
        PublicAdditionalLibraries.Add(dependencyLibrary);
    }
}
