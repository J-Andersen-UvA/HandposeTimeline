using UnrealBuildTool;

public class AnimationPlacement : ModuleRules
{
    public AnimationPlacement(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "AdditiveBoneCorrection",
                "AdditiveBoneCorrectionEditor",
                "AnimationBlueprintLibrary",
                "Core",
                "CoreUObject",
                "Engine"
            });

        PrivateDependencyModuleNames.Add("AnimationDataController");
    }
}
