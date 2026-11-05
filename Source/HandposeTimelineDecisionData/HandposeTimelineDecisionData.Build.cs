using UnrealBuildTool;

public class HandposeTimelineDecisionData : ModuleRules
{
    public HandposeTimelineDecisionData(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AssetRegistry",
                "Json",
                "JsonUtilities",
                "UnrealEd"
            });
    }
}
