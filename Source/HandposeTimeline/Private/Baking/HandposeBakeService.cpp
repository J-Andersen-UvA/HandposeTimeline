#include "Baking/HandposeBakeService.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Misc/DateTime.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

FHandposeBakeResult FHandposeBakeService::bake(
    UAnimSequence* previewAnimation,
    const UAnimSequence* sourceAnimation,
    const FString& outputRoot,
    const int32 trimStartFrame,
    const int32 trimEndFrame)
{
    FHandposeBakeResult result;
    if (previewAnimation == nullptr || sourceAnimation == nullptr)
    {
        result.errorMessage = TEXT("A source animation and generated preview are required.");
        return result;
    }
    if (outputRoot.IsEmpty())
    {
        result.errorMessage = TEXT("Set Baked Animation Output Folder in the loaded config.");
        return result;
    }

    FString normalizedRoot = outputRoot;
    normalizedRoot.RemoveFromEnd(TEXT("/"));
    FString datedFolder = normalizedRoot /
        FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
    datedFolder.ReplaceInline(TEXT("//"), TEXT("/"));
    FText invalidPathReason;
    if (!FPackageName::IsValidLongPackageName(
            datedFolder,
            true,
            &invalidPathReason))
    {
        result.errorMessage = FString::Printf(
            TEXT("Invalid bake output folder: %s"),
            *invalidPathReason.ToString());
        return result;
    }
    if (!UEditorAssetLibrary::DoesDirectoryExist(datedFolder) &&
        !UEditorAssetLibrary::MakeDirectory(datedFolder))
    {
        result.errorMessage = FString::Printf(
            TEXT("Could not create bake output folder %s."),
            *datedFolder);
        return result;
    }

    const FString assetName = sourceAnimation->GetName();
    const FString assetPath = datedFolder / assetName;
    if (UEditorAssetLibrary::DoesAssetExist(assetPath))
    {
        const EAppReturnType::Type overwriteResponse = FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::Format(
                NSLOCTEXT(
                    "HandposeBakeService",
                    "ConfirmOverwrite",
                    "An animation already exists at {0}. Overwrite it?"),
                FText::FromString(assetPath)));
        if (overwriteResponse != EAppReturnType::Yes)
        {
            result.cancelled = true;
            return result;
        }
        if (!UEditorAssetLibrary::DeleteAsset(assetPath))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not overwrite existing animation %s."),
                *assetPath);
            return result;
        }
    }

    UPackage* package = CreatePackage(*assetPath);
    UAnimSequence* outputAnimation = DuplicateObject<UAnimSequence>(
        previewAnimation,
        package,
        FName(*assetName));
    if (outputAnimation == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the baked animation.");
        return result;
    }
    outputAnimation->ClearFlags(RF_Transient);
    outputAnimation->SetFlags(RF_Public | RF_Standalone | RF_Transactional);

    const int32 lastFrame = FMath::Max(0, outputAnimation->GetNumberOfSampledKeys() - 1);
    const int32 clampedStartFrame = FMath::Clamp(trimStartFrame, 0, lastFrame);
    const int32 clampedEndFrame = FMath::Clamp(trimEndFrame, clampedStartFrame, lastFrame);
    if (clampedEndFrame < lastFrame)
    {
        UE::Anim::AnimationData::Trim(
            outputAnimation,
            TRange<FFrameNumber>(
                TRangeBound<FFrameNumber>::Inclusive(clampedEndFrame + 1),
                TRangeBound<FFrameNumber>::Exclusive(lastFrame + 1)));
    }
    if (clampedStartFrame > 0)
    {
        UE::Anim::AnimationData::Trim(
            outputAnimation,
            TRange<FFrameNumber>(
                TRangeBound<FFrameNumber>::Inclusive(0),
                TRangeBound<FFrameNumber>::Exclusive(clampedStartFrame)));
    }

    outputAnimation->MarkPackageDirty();
    outputAnimation->PostEditChange();
    FAssetRegistryModule::AssetCreated(outputAnimation);
    if (!UEditorAssetLibrary::SaveLoadedAsset(outputAnimation, false))
    {
        result.errorMessage = FString::Printf(
            TEXT("Created %s but could not save it."),
            *assetPath);
        return result;
    }

    result.success = true;
    result.animation = outputAnimation;
    result.outputPath = assetPath;
    return result;
}
