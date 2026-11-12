#include "Handposes/HandposeMirrorService.h"

#include "AnimPose.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimationPoseUtilities.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

#define LOCTEXT_NAMESPACE "HandposeMirrorService"

namespace
{
struct FBonePair
{
    FName source;
    FName target;

    bool operator==(const FBonePair& other) const
    {
        return source == other.source && target == other.target;
    }
};

void addPair(
    TArray<FBonePair>& pairs,
    const FName source,
    const FName target)
{
    if (!source.IsNone() && !target.IsNone())
    {
        pairs.AddUnique({source, target});
    }
}

TArray<FBonePair> makeBonePairs(
    const FHandshapeBoneMapping& source,
    const FHandshapeBoneMapping& target)
{
    TArray<FBonePair> pairs;
    addPair(pairs, source.thumb.metacarpal, target.thumb.metacarpal);
    addPair(pairs, source.thumb.proximal, target.thumb.proximal);
    addPair(pairs, source.thumb.distal, target.thumb.distal);
    addPair(pairs, source.thumb.tip, target.thumb.tip);

    const FHandshapeFingerBoneMapping* sourceFingers[] = {
        &source.index, &source.middle, &source.ring, &source.pinky};
    const FHandshapeFingerBoneMapping* targetFingers[] = {
        &target.index, &target.middle, &target.ring, &target.pinky};
    for (int32 fingerIndex = 0; fingerIndex < 4; ++fingerIndex)
    {
        addPair(pairs, sourceFingers[fingerIndex]->metacarpal, targetFingers[fingerIndex]->metacarpal);
        addPair(pairs, sourceFingers[fingerIndex]->proximal, targetFingers[fingerIndex]->proximal);
        addPair(pairs, sourceFingers[fingerIndex]->middle, targetFingers[fingerIndex]->middle);
        addPair(pairs, sourceFingers[fingerIndex]->distal, targetFingers[fingerIndex]->distal);
        addPair(pairs, sourceFingers[fingerIndex]->tip, targetFingers[fingerIndex]->tip);
    }
    return pairs;
}

}

FHandposeMirrorResult FHandposeMirrorService::remapAnimationToOppositeHand(
    UAnimSequence* sourceAnimation,
    const FHandshapeBoneMapping& sourceMapping,
    const FHandshapeBoneMapping& targetMapping,
    const EAxis::Type flipAxis)
{
    FHandposeMirrorResult result;
    if (sourceAnimation == nullptr || sourceAnimation->GetSkeleton() == nullptr)
    {
        result.errorMessage = TEXT("A handpose animation with a skeleton is required for remapping.");
        return result;
    }

    const TArray<FBonePair> pairs = makeBonePairs(sourceMapping, targetMapping);
    if (pairs.IsEmpty())
    {
        result.errorMessage = TEXT("The source and target hand bone mappings do not contain matching bones.");
        return result;
    }

    const FReferenceSkeleton& referenceSkeleton =
        sourceAnimation->GetSkeleton()->GetReferenceSkeleton();
    for (const FBonePair& pair : pairs)
    {
        if (referenceSkeleton.FindBoneIndex(pair.source) == INDEX_NONE ||
            referenceSkeleton.FindBoneIndex(pair.target) == INDEX_NONE)
        {
            result.errorMessage = FString::Printf(
                TEXT("Cannot remap %s to %s because one of the mapped bones is missing."),
                *pair.source.ToString(),
                *pair.target.ToString());
            return result;
        }
    }

    TArray<FAnimPose> poses;
    if (!HandposeTimeline::AnimationPoseUtilities::evaluatePoses(
            *sourceAnimation,
            poses,
            result.errorMessage))
    {
        return result;
    }
    const int32 frameCount = poses.Num();

    UAnimSequence* remappedAnimation =
        HandposeTimeline::AnimationPoseUtilities::duplicateTransientAnimation(
            *sourceAnimation,
            TEXT("RemappedHandpose"));
    if (remappedAnimation == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the remapped handpose animation.");
        return result;
    }

    IAnimationDataController& controller = remappedAnimation->GetController();
    IAnimationDataModel* dataModel = remappedAnimation->GetDataModel();
    IAnimationDataController::FScopedBracket bracket(
        controller,
        LOCTEXT("RemapHandpose", "Remap handpose"),
        false);

    for (const FBonePair& pair : pairs)
    {
        const int32 sourceIndex = referenceSkeleton.FindBoneIndex(pair.source);
        const int32 targetIndex = referenceSkeleton.FindBoneIndex(pair.target);
        const FTransform& sourceReference =
            referenceSkeleton.GetRawRefBonePose()[sourceIndex];
        const FTransform& targetReference =
            referenceSkeleton.GetRawRefBonePose()[targetIndex];
        TArray<FVector> translations;
        TArray<FQuat> rotations;
        TArray<FVector> scales;
        translations.Reserve(frameCount);
        rotations.Reserve(frameCount);
        scales.Reserve(frameCount);
        for (const FAnimPose& pose : poses)
        {
            const FTransform sourceLocal = UAnimPoseExtensions::GetBonePose(
                pose,
                pair.source,
                EAnimPoseSpaces::Local);
            FTransform mirroredDelta =
                sourceLocal.GetRelativeTransform(sourceReference);
            if (flipAxis != EAxis::None)
            {
                mirroredDelta.Mirror(EAxis::Y, flipAxis);
            }
            const FTransform targetLocal = mirroredDelta * targetReference;
            translations.Add(targetLocal.GetTranslation());
            rotations.Add(targetLocal.GetRotation().GetNormalized());
            scales.Add(targetLocal.GetScale3D());
        }

        if (!dataModel->IsValidBoneTrackName(pair.target) &&
            !controller.AddBoneCurve(pair.target, false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not add mirrored bone track %s."),
                *pair.target.ToString());
            return result;
        }
        if (!controller.SetBoneTrackKeys(
                pair.target,
                translations,
                rotations,
                scales,
                false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not write mirrored bone track %s."),
                *pair.target.ToString());
            return result;
        }
    }

    remappedAnimation->PostEditChange();
    result.success = true;
    result.animation = remappedAnimation;
    return result;
}

TArray<FName> FHandposeMirrorService::getTargetBoneNames(
    const FHandshapeBoneMapping& sourceMapping,
    const FHandshapeBoneMapping& targetMapping)
{
    TArray<FName> targetBoneNames;
    for (const FBonePair& pair : makeBonePairs(sourceMapping, targetMapping))
    {
        targetBoneNames.AddUnique(pair.target);
    }
    return targetBoneNames;
}

#undef LOCTEXT_NAMESPACE
