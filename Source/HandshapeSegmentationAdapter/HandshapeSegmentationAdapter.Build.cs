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

        string dependencyLibrary = Path.Combine(
            ModuleDirectory,
            "Lib",
            "Win64",
            "HandshapeSegmentationUpstream.lib");
        if (!File.Exists(dependencyLibrary))
        {
            throw new BuildException(
                "Build Source/HandshapeSegmentationAdapter before compiling HandposeTimeline. " +
                "See Source/HandshapeSegmentationAdapter/README.md.");
        }
        PublicAdditionalLibraries.Add(dependencyLibrary);
    }
}
