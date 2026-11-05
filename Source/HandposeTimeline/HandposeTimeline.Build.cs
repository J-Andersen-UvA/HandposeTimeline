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
                "AnimationDataController",
                "AssetRegistry",
                "ContentBrowser",
                "CoreUObject",
                "DesktopPlatform",
                "Engine",
                "EditorScriptingUtilities",
                "HandshapeSegmentationAdapter",
                "HandposeTimelineDecisionData",
                "InputCore",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            });
    }
}
