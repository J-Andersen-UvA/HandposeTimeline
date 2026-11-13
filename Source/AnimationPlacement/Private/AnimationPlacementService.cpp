#include "AnimationPlacementService.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

#define LOCTEXT_NAMESPACE "AnimationPlacementService"

FAnimationPlacementResult FAnimationPlacementService::rotate180(
    UAnimSequence* sourceAnimation)
{
    FAnimationPlacementResult result;
    if (sourceAnimation == nullptr || sourceAnimation->GetSkeleton() == nullptr)
    {
        result.errorMessage = TEXT("A source animation with a skeleton is required.");
        return result;
    }

    const FReferenceSkeleton& referenceSkeleton =
        sourceAnimation->GetSkeleton()->GetReferenceSkeleton();
    if (referenceSkeleton.GetNum() == 0)
    {
        result.errorMessage = TEXT("The source animation skeleton has no bones.");
        return result;
    }

    UAnimSequence* rotatedAnimation = DuplicateObject<UAnimSequence>(
        sourceAnimation,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UAnimSequence::StaticClass(),
            TEXT("HandposeTimelineRotate180")));
    if (rotatedAnimation == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the rotated preview animation.");
        return result;
    }
    rotatedAnimation->ClearFlags(RF_Public | RF_Standalone);
    rotatedAnimation->SetFlags(RF_Transient);

    const FName rootBoneName = referenceSkeleton.GetBoneName(0);
    IAnimationDataModel* dataModel = rotatedAnimation->GetDataModel();
    IAnimationDataController& controller = rotatedAnimation->GetController();
    const bool hasRootTrack = dataModel->IsValidBoneTrackName(rootBoneName);

    TArray<FTransform> rootTransforms;
    if (hasRootTrack)
    {
        dataModel->GetBoneTrackTransforms(rootBoneName, rootTransforms);
    }
    else
    {
        rootTransforms.Init(
            referenceSkeleton.GetRefBonePose()[0],
            rotatedAnimation->GetNumberOfSampledKeys());
    }
    if (rootTransforms.IsEmpty())
    {
        result.errorMessage = TEXT("The source animation has no root-bone samples.");
        return result;
    }

    const FQuat rotationOffset(FVector::UpVector, PI);
    TArray<FVector> translations;
    TArray<FQuat> rotations;
    TArray<FVector> scales;
    translations.Reserve(rootTransforms.Num());
    rotations.Reserve(rootTransforms.Num());
    scales.Reserve(rootTransforms.Num());
    for (const FTransform& rootTransform : rootTransforms)
    {
        translations.Add(rotationOffset.RotateVector(rootTransform.GetTranslation()));
        rotations.Add(
            (rotationOffset * rootTransform.GetRotation()).GetNormalized());
        scales.Add(rootTransform.GetScale3D());
    }

    IAnimationDataController::FScopedBracket bracket(
        controller,
        LOCTEXT("RotateAnimation180", "Rotate animation 180 degrees"),
        false);
    if (!hasRootTrack && !controller.AddBoneCurve(rootBoneName, false))
    {
        result.errorMessage = FString::Printf(
            TEXT("Could not add the root track '%s'."),
            *rootBoneName.ToString());
        return result;
    }
    if (!controller.SetBoneTrackKeys(
            rootBoneName,
            translations,
            rotations,
            scales,
            false))
    {
        result.errorMessage = FString::Printf(
            TEXT("Could not write the rotated root track '%s'."),
            *rootBoneName.ToString());
        return result;
    }

    rotatedAnimation->PostEditChange();
    result.success = true;
    result.animation = rotatedAnimation;
    return result;
}

#undef LOCTEXT_NAMESPACE
