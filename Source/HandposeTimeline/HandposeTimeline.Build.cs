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
                "AnimationPlacement",
                "AdditiveBoneCorrection",
                "AdditiveBoneCorrectionEditor",
                "AssetRegistry",
                "ContentBrowser",
                "CoreUObject",
                "DesktopPlatform",
                "Engine",
                "EditorScriptingUtilities",
                "FaceTouchCorrection",
                "FaceTouchCorrectionEditor",
                "HandshapeSegmentationAdapter",
                "HandposeTimelineDecisionData",
                "InputCore",
                "SemanticPatchProximity",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            });
    }
}
