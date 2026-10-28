using UnrealBuildTool;

public class HandposeTimeline : ModuleRules
{
    public HandposeTimeline(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AnimationBlueprintLibrary",
                "AssetRegistry",
                "ContentBrowser",
                "CoreUObject",
                "Engine",
                "HandshapeSegmentationAdapter",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            });
    }
}
