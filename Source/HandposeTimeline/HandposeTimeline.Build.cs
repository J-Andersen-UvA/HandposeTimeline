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
                "Engine",
                "EditorScriptingUtilities",
                "HandshapeSegmentationAdapter",
                "InputCore",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            });
    }
}
