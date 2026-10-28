using UnrealBuildTool;

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
    }
}
