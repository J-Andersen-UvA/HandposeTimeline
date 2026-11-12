#include "FaceTouch/FaceTouchPreviewBuilder.h"

#include "AnimPose.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimationPoseUtilities.h"
#include "Animation/AnimSequence.h"
#include "BoneCorrectionEditorLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "SemanticInteractionPointComponent.h"

#define LOCTEXT_NAMESPACE "FaceTouchPreviewBuilder"

namespace
{
bool isFingerBoneName(const FString& boneName)
{
    return boneName.Contains(TEXT("thumb")) ||
        boneName.Contains(TEXT("index")) ||
        boneName.Contains(TEXT("middle")) ||
        boneName.Contains(TEXT("ring")) ||
        boneName.Contains(TEXT("pinky")) ||
        boneName.Contains(TEXT("little"));
}

bool buildArmChain(
    const USkeletalMesh& skeletalMesh,
    const FName interactionBone,
    FBoneCorrectionChain& outChain,
    FString& errorMessage)
{
    const FReferenceSkeleton& referenceSkeleton = skeletalMesh.GetRefSkeleton();
    int32 boneIndex = referenceSkeleton.FindBoneIndex(interactionBone);
    int32 handIndex = INDEX_NONE;
    while (boneIndex != INDEX_NONE)
    {
        const FString boneName = referenceSkeleton.GetBoneName(boneIndex)
            .ToString()
            .ToLower();
        if (boneName.Contains(TEXT("hand")) && !isFingerBoneName(boneName))
        {
            handIndex = boneIndex;
            break;
        }
        boneIndex = referenceSkeleton.GetParentIndex(boneIndex);
    }
    if (handIndex == INDEX_NONE)
    {
        errorMessage = FString::Printf(
            TEXT("Could not find the hand bone above interaction bone '%s'."),
            *interactionBone.ToString());
        return false;
    }

    const int32 forearmIndex = referenceSkeleton.GetParentIndex(handIndex);
    const int32 upperArmIndex = forearmIndex != INDEX_NONE
        ? referenceSkeleton.GetParentIndex(forearmIndex)
        : INDEX_NONE;
    if (forearmIndex == INDEX_NONE || upperArmIndex == INDEX_NONE)
    {
        errorMessage = TEXT("The interaction point does not have a complete arm chain.");
        return false;
    }

    outChain.bones = {
        referenceSkeleton.GetBoneName(upperArmIndex),
        referenceSkeleton.GetBoneName(forearmIndex),
        referenceSkeleton.GetBoneName(handIndex)};
    return true;
}

const FBoneCorrectionBoneDelta* findDelta(
    const TArray<FBoneCorrectionBoneDelta>& deltas,
    const FName boneName)
{
    return deltas.FindByPredicate(
        [boneName](const FBoneCorrectionBoneDelta& delta)
        {
            return delta.boneName == boneName;
        });
}

USemanticInteractionPointComponent* findInteractionPoint(
    AActor& actor,
    const FName pointId)
{
    TArray<USemanticInteractionPointComponent*> points;
    actor.GetComponents(points);
    USemanticInteractionPointComponent** result = points.FindByPredicate(
        [pointId](const USemanticInteractionPointComponent* point)
        {
            return point != nullptr && point->pointId == pointId;
        });
    return result != nullptr ? *result : nullptr;
}

USkeletalMeshComponent* findSkeletalMeshComponent(
    AActor& actor,
    const FName componentName)
{
    TArray<USkeletalMeshComponent*> components;
    actor.GetComponents(components);
    USkeletalMeshComponent** result = components.FindByPredicate(
        [componentName](const USkeletalMeshComponent* component)
        {
            return component != nullptr && component->GetFName() == componentName;
        });
    return result != nullptr ? *result : nullptr;
}

struct FSolvedFaceTouchCorrection
{
    const FFaceTouchCorrectionBlock* correction = nullptr;
    FBoneCorrectionIkResult solveResult;
};
}

FHandposePreviewBuildResult FFaceTouchPreviewBuilder::applyCorrections(
    UAnimSequence* sourceAnimation,
    AActor* actor,
    const TArray<FFaceTouchCorrectionBlock>& corrections)
{
    FHandposePreviewBuildResult result;
    if (sourceAnimation == nullptr || actor == nullptr || corrections.IsEmpty())
    {
        result.errorMessage = TEXT("Animation, actor, and face-touch corrections are required.");
        return result;
    }

    TArray<FSolvedFaceTouchCorrection> solvedCorrections;
    TSet<FName> correctedBoneNames;
    for (const FFaceTouchCorrectionBlock& correction : corrections)
    {
        if (!correction.isValid() || correction.interactionPointIds.IsEmpty())
        {
            continue;
        }

        USemanticInteractionPointComponent* point = findInteractionPoint(
            *actor,
            correction.interactionPointIds[0]);
        USkeletalMeshComponent* interactionMesh = point != nullptr
            ? Cast<USkeletalMeshComponent>(point->GetAttachParent())
            : nullptr;
        USkeletalMeshComponent* surfaceMesh = findSkeletalMeshComponent(
            *actor,
            correction.surfaceLocation.skeletalMeshComponentName);
        const USkeletalMesh* skeletalMesh = interactionMesh != nullptr
            ? interactionMesh->GetSkeletalMeshAsset()
            : nullptr;
        if (point == nullptr || interactionMesh == nullptr ||
            surfaceMesh == nullptr || skeletalMesh == nullptr)
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not resolve '%s' and its skeletal mesh components."),
                *correction.interactionPointIds[0].ToString());
            return result;
        }

        const FSemanticInteractionPoint interactionPoint = point->toInteractionPoint();
        FBoneCorrectionChain chain;
        if (!buildArmChain(
                *skeletalMesh,
                interactionPoint.parentBone,
                chain,
                result.errorMessage))
        {
            return result;
        }

        const FVector componentLocalGoal =
            correction.surfaceLocation.componentLocalPosition -
            correction.surfaceLocation.componentLocalNormal.GetSafeNormal() *
                correction.surfaceLocation.normalOffset;
        const FVector surfaceWorld = surfaceMesh->GetComponentTransform()
            .TransformPosition(componentLocalGoal);
        const FVector componentSpaceGoal = interactionMesh->GetComponentTransform()
            .InverseTransformPosition(surfaceWorld);
        FBoneCorrectionIkSettings settings;
        FSolvedFaceTouchCorrection& solved = solvedCorrections.AddDefaulted_GetRef();
        solved.correction = &correction;
        const int32 solveFrame = correction.solveFrame >= 0
            ? correction.solveFrame
            : correction.touchStartFrame;
        solved.solveResult = UBoneCorrectionEditorLibrary::solvePositionIkAtFrame(
            sourceAnimation,
            skeletalMesh,
            solveFrame,
            interactionPoint.parentBone,
            interactionPoint.boneLocalPosition,
            componentSpaceGoal,
            chain,
            settings);
        if (!solved.solveResult.isValid)
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not solve '%s': %s"),
                *correction.interactionPointIds[0].ToString(),
                *solved.solveResult.errorMessage);
            return result;
        }
        for (const FBoneCorrectionBoneDelta& delta :
             solved.solveResult.correctionKey.boneDeltas)
        {
            correctedBoneNames.Add(delta.boneName);
        }
    }
    if (solvedCorrections.IsEmpty() || correctedBoneNames.IsEmpty())
    {
        result.errorMessage = TEXT("No face-touch correction could be solved.");
        return result;
    }

    TArray<FAnimPose> poses;
    if (!HandposeTimeline::AnimationPoseUtilities::evaluatePoses(
            *sourceAnimation,
            poses,
            result.errorMessage))
    {
        return result;
    }

    const int32 frameCount = sourceAnimation->GetNumberOfSampledKeys();
    TMap<FName, TArray<FVector>> translationsByBone;
    TMap<FName, TArray<FQuat>> rotationsByBone;
    TMap<FName, TArray<FVector>> scalesByBone;
    for (const FName boneName : correctedBoneNames)
    {
        translationsByBone.Add(boneName).Reserve(frameCount);
        rotationsByBone.Add(boneName).Reserve(frameCount);
        scalesByBone.Add(boneName).Reserve(frameCount);
    }

    for (int32 frame = 0; frame < frameCount; ++frame)
    {
        for (const FName boneName : correctedBoneNames)
        {
            FTransform localTransform = UAnimPoseExtensions::GetBonePose(
                poses[frame],
                boneName,
                EAnimPoseSpaces::Local);
            double totalWeight = 0.0;
            FVector weightedTranslation = FVector::ZeroVector;
            FVector weightedScale = FVector::ZeroVector;
            FQuat blendedRotation = FQuat::Identity;
            bool hasRotation = false;
            for (const FSolvedFaceTouchCorrection& solved : solvedCorrections)
            {
                const double weight = solved.correction->evaluateWeight(frame);
                const FBoneCorrectionBoneDelta* delta = findDelta(
                    solved.solveResult.correctionKey.boneDeltas,
                    boneName);
                if (delta == nullptr || weight <= UE_SMALL_NUMBER)
                {
                    continue;
                }
                weightedTranslation += delta->translationDelta * weight;
                weightedScale += delta->scaleMultiplier * weight;
                if (!hasRotation)
                {
                    blendedRotation = delta->rotationDelta;
                    hasRotation = true;
                }
                else
                {
                    blendedRotation = FQuat::Slerp(
                        blendedRotation,
                        delta->rotationDelta,
                        weight / (totalWeight + weight)).GetNormalized();
                }
                totalWeight += weight;
            }
            if (totalWeight > UE_SMALL_NUMBER)
            {
                const double combinedInfluence = FMath::Min(1.0, totalWeight);
                const FVector averageTranslation =
                    weightedTranslation / totalWeight;
                const FVector averageScale = weightedScale / totalWeight;
                localTransform.AddToTranslation(
                    averageTranslation * combinedInfluence);
                localTransform.SetRotation(
                    (FQuat::Slerp(
                        FQuat::Identity,
                        blendedRotation,
                        combinedInfluence) * localTransform.GetRotation())
                      .GetNormalized());
                localTransform.SetScale3D(
                    localTransform.GetScale3D() * FMath::Lerp(
                        FVector::OneVector,
                        averageScale,
                        combinedInfluence));
            }
            translationsByBone.FindChecked(boneName).Add(
                localTransform.GetTranslation());
            rotationsByBone.FindChecked(boneName).Add(
                localTransform.GetRotation());
            scalesByBone.FindChecked(boneName).Add(
                localTransform.GetScale3D());
        }
    }

    UAnimSequence* preview =
        HandposeTimeline::AnimationPoseUtilities::duplicateTransientAnimation(
            *sourceAnimation,
            TEXT("HandposeTimelineFaceTouchPreview"));
    if (preview == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the face-touch preview animation.");
        return result;
    }

    IAnimationDataController& controller = preview->GetController();
    IAnimationDataModel* dataModel = preview->GetDataModel();
    IAnimationDataController::FScopedBracket bracket(
        controller,
        LOCTEXT("BuildFaceTouchPreview", "Build face-touch preview"),
        false);
    for (const FName boneName : correctedBoneNames)
    {
        if (!dataModel->IsValidBoneTrackName(boneName) &&
            !controller.AddBoneCurve(boneName, false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not add face-touch preview track %s."),
                *boneName.ToString());
            return result;
        }
        if (!controller.SetBoneTrackKeys(
                boneName,
                translationsByBone.FindChecked(boneName),
                rotationsByBone.FindChecked(boneName),
                scalesByBone.FindChecked(boneName),
                false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not write face-touch preview track %s."),
                *boneName.ToString());
            return result;
        }
    }
    preview->PostEditChange();
    result.success = true;
    result.animation = preview;
    return result;
}

#undef LOCTEXT_NAMESPACE
