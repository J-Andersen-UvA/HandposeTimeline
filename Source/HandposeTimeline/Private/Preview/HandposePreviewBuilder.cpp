#include "Preview/HandposePreviewBuilder.h"

#include "AnimPose.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "HandposeAssignment.h"

#define LOCTEXT_NAMESPACE "HandposePreviewBuilder"

namespace
{
struct FActiveHandshape
{
    const FHandposeAssignment* assignment = nullptr;
    float weight = 0.0f;
};

bool isSkippedFinger(
    const FName boneName,
    const TArray<EHandposeFinger>& skippedFingers)
{
    const FString normalizedBoneName = boneName.ToString().ToLower();
    return skippedFingers.ContainsByPredicate(
        [&normalizedBoneName](const EHandposeFinger finger)
        {
            switch (finger)
            {
            case EHandposeFinger::Thumb:
                return normalizedBoneName.Contains(TEXT("thumb"));
            case EHandposeFinger::Index:
                return normalizedBoneName.Contains(TEXT("index"));
            case EHandposeFinger::Middle:
                return normalizedBoneName.Contains(TEXT("middle"));
            case EHandposeFinger::Ring:
                return normalizedBoneName.Contains(TEXT("ring"));
            case EHandposeFinger::Pinky:
                return normalizedBoneName.Contains(TEXT("pinky")) ||
                    normalizedBoneName.Contains(TEXT("little"));
            default:
                return false;
            }
        });
}

FAnimPoseEvaluationOptions makeEvaluationOptions()
{
    FAnimPoseEvaluationOptions options;
    options.EvaluationType = EAnimDataEvalType::Raw;
    options.bShouldRetarget = true;
    options.bEvaluateCurves = false;
    return options;
}

bool evaluatePoses(
    UAnimSequenceBase& animation,
    TArray<FAnimPose>& poses,
    FString& errorMessage)
{
    const int32 frameCount = animation.GetNumberOfSampledKeys();
    const FFrameRate frameRate = animation.GetSamplingFrameRate();
    if (frameCount < 1 || !frameRate.IsValid())
    {
        errorMessage = FString::Printf(
            TEXT("Animation %s has no sampled frames or has an invalid frame rate."),
            *animation.GetName());
        return false;
    }

    TArray<double> times;
    times.Reserve(frameCount);
    for (int32 frame = 0; frame < frameCount; ++frame)
    {
        times.Add(frameRate.AsSeconds(frame));
    }
    UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(
        &animation,
        times,
        makeEvaluationOptions(),
        poses);
    if (poses.Num() != frameCount || poses.ContainsByPredicate(
            [](const FAnimPose& pose)
            {
                return !UAnimPoseExtensions::IsValid(pose);
            }))
    {
        errorMessage = FString::Printf(
            TEXT("Unreal could not evaluate every pose in %s."),
            *animation.GetName());
        return false;
    }
    return true;
}

bool calculateAverageRotations(
    UAnimSequenceBase& animation,
    const TArray<FName>& boneNames,
    TMap<FName, FQuat>& rotations,
    FString& errorMessage)
{
    const USkeleton* skeleton = animation.GetSkeleton();
    if (skeleton == nullptr)
    {
        errorMessage = FString::Printf(
            TEXT("Handshape %s has no skeleton."),
            *animation.GetName());
        return false;
    }
    const FReferenceSkeleton& referenceSkeleton = skeleton->GetReferenceSkeleton();
    for (const FName boneName : boneNames)
    {
        if (referenceSkeleton.FindBoneIndex(boneName) == INDEX_NONE)
        {
            errorMessage = FString::Printf(
                TEXT("Handshape %s does not contain bone %s."),
                *animation.GetName(),
                *boneName.ToString());
            return false;
        }
    }

    TArray<FAnimPose> poses;
    if (!evaluatePoses(animation, poses, errorMessage))
    {
        return false;
    }

    for (const FName boneName : boneNames)
    {
        FQuat reference = UAnimPoseExtensions::GetBonePose(
            poses[0],
            boneName,
            EAnimPoseSpaces::Local).GetRotation().GetNormalized();
        FVector4 sum(reference.X, reference.Y, reference.Z, reference.W);
        for (int32 poseIndex = 1; poseIndex < poses.Num(); ++poseIndex)
        {
            FQuat rotation = UAnimPoseExtensions::GetBonePose(
                poses[poseIndex],
                boneName,
                EAnimPoseSpaces::Local).GetRotation().GetNormalized();
            if ((reference | rotation) < 0.0f)
            {
                rotation.X *= -1.0f;
                rotation.Y *= -1.0f;
                rotation.Z *= -1.0f;
                rotation.W *= -1.0f;
            }
            sum += FVector4(rotation.X, rotation.Y, rotation.Z, rotation.W);
        }
        rotations.Add(
            boneName,
            FQuat(sum.X, sum.Y, sum.Z, sum.W).GetNormalized());
    }
    return true;
}

FQuat evaluateRotation(
    const FQuat& baseRotation,
    const TArray<FActiveHandshape>& activeHandshapes,
    const TArray<TMap<FName, FQuat>>& rotationsByAssignment,
    const FName boneName,
    const TArray<FHandposeAssignment>& assignments)
{
    if (activeHandshapes.IsEmpty())
    {
        return baseRotation;
    }
    if (activeHandshapes.Num() == 1)
    {
        const int32 assignmentIndex = static_cast<int32>(
            activeHandshapes[0].assignment - assignments.GetData());
        const FQuat target = rotationsByAssignment[assignmentIndex].FindChecked(boneName);
        return FQuat::Slerp(
            baseRotation,
            target,
            activeHandshapes[0].weight).GetNormalized();
    }

    const FActiveHandshape& first = activeHandshapes[0];
    int32 assignmentIndex = static_cast<int32>(first.assignment - assignments.GetData());
    FQuat result = rotationsByAssignment[assignmentIndex].FindChecked(boneName);
    float accumulatedWeight = first.weight;
    for (int32 activeIndex = 1; activeIndex < activeHandshapes.Num(); ++activeIndex)
    {
        const FActiveHandshape& active = activeHandshapes[activeIndex];
        assignmentIndex = static_cast<int32>(active.assignment - assignments.GetData());
        const FQuat target = rotationsByAssignment[assignmentIndex].FindChecked(boneName);
        const float nextTotal = accumulatedWeight + active.weight;
        const float alpha = nextTotal > UE_SMALL_NUMBER
            ? active.weight / nextTotal
            : 0.0f;
        result = FQuat::Slerp(result, target, alpha).GetNormalized();
        accumulatedWeight = nextTotal;
    }
    return result;
}
}

FHandposePreviewBuildResult FHandposePreviewBuilder::build(
    UAnimSequence* sourceAnimation,
    const TArray<FHandposeAssignment>& assignments,
    const TArray<FName>& rightBoneNames,
    const TArray<FName>& leftBoneNames)
{
    FHandposePreviewBuildResult result;
    if (sourceAnimation == nullptr || assignments.IsEmpty())
    {
        result.errorMessage = TEXT("Source animation, assignments, and hand bones are required.");
        return result;
    }
    TArray<FName> boneNames = rightBoneNames;
    for (const FName boneName : leftBoneNames)
    {
        boneNames.AddUnique(boneName);
    }
    if (boneNames.IsEmpty())
    {
        result.errorMessage = TEXT("No hand bones are configured.");
        return result;
    }
    const USkeleton* sourceSkeleton = sourceAnimation->GetSkeleton();
    if (sourceSkeleton == nullptr)
    {
        result.errorMessage = TEXT("The source animation has no skeleton.");
        return result;
    }
    for (const FName boneName : boneNames)
    {
        if (sourceSkeleton->GetReferenceSkeleton().FindBoneIndex(boneName) == INDEX_NONE)
        {
            result.errorMessage = FString::Printf(
                TEXT("The source animation does not contain bone %s."),
                *boneName.ToString());
            return result;
        }
    }

    TArray<FAnimPose> sourcePoses;
    if (!evaluatePoses(*sourceAnimation, sourcePoses, result.errorMessage))
    {
        return result;
    }

    TArray<TMap<FName, FQuat>> rotationsByAssignment;
    rotationsByAssignment.SetNum(assignments.Num());
    for (int32 assignmentIndex = 0; assignmentIndex < assignments.Num(); ++assignmentIndex)
    {
        const TArray<FName>& handBoneNames =
            assignments[assignmentIndex].handSide == EHandposeSide::Left
                ? leftBoneNames
                : rightBoneNames;
        if (handBoneNames.IsEmpty())
        {
            result.errorMessage = TEXT("An assigned handshape has no configured hand bones.");
            return result;
        }
        TArray<FName> assignmentBoneNames;
        for (const FName boneName : handBoneNames)
        {
            if (!isSkippedFinger(
                    boneName,
                    assignments[assignmentIndex].skippedFingers))
            {
                assignmentBoneNames.Add(boneName);
            }
        }
        UAnimSequenceBase* handshapeAnimation =
            assignments[assignmentIndex].handshapeAnimation.LoadSynchronous();
        if (handshapeAnimation == nullptr ||
            !calculateAverageRotations(
                *handshapeAnimation,
                assignmentBoneNames,
                rotationsByAssignment[assignmentIndex],
                result.errorMessage))
        {
            return result;
        }
    }

    const int32 frameCount = sourceAnimation->GetNumberOfSampledKeys();
    TMap<FName, TArray<FVector>> translationsByBone;
    TMap<FName, TArray<FQuat>> rotationsByBone;
    TMap<FName, TArray<FVector>> scalesByBone;
    for (const FName boneName : boneNames)
    {
        translationsByBone.Add(boneName).Reserve(frameCount);
        rotationsByBone.Add(boneName).Reserve(frameCount);
        scalesByBone.Add(boneName).Reserve(frameCount);
    }

    for (int32 frame = 0; frame < frameCount; ++frame)
    {
        TArray<FActiveHandshape> activeHandshapes;
        for (const FHandposeAssignment& assignment : assignments)
        {
            const float weight = assignment.evaluateWeight(frame);
            if (weight > UE_SMALL_NUMBER)
            {
                activeHandshapes.Add({&assignment, weight});
            }
        }
        activeHandshapes.Sort(
            [](const FActiveHandshape& left, const FActiveHandshape& right)
            {
                return left.assignment->blendStartFrame < right.assignment->blendStartFrame;
            });

        for (const FName boneName : boneNames)
        {
            TArray<FActiveHandshape> activeHandshapesForBone;
            for (const FActiveHandshape& activeHandshape : activeHandshapes)
            {
                const int32 assignmentIndex = static_cast<int32>(
                    activeHandshape.assignment - assignments.GetData());
                if (rotationsByAssignment[assignmentIndex].Contains(boneName))
                {
                    activeHandshapesForBone.Add(activeHandshape);
                }
            }
            const FTransform baseTransform = UAnimPoseExtensions::GetBonePose(
                sourcePoses[frame],
                boneName,
                EAnimPoseSpaces::Local);
            translationsByBone.FindChecked(boneName).Add(baseTransform.GetTranslation());
            rotationsByBone.FindChecked(boneName).Add(evaluateRotation(
                baseTransform.GetRotation(),
                activeHandshapesForBone,
                rotationsByAssignment,
                boneName,
                assignments));
            scalesByBone.FindChecked(boneName).Add(baseTransform.GetScale3D());
        }
    }

    UAnimSequence* preview = DuplicateObject<UAnimSequence>(
        sourceAnimation,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UAnimSequence::StaticClass(),
            TEXT("HandposeTimelinePreview")));
    if (preview == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the transient preview animation.");
        return result;
    }
    preview->ClearFlags(RF_Public | RF_Standalone);
    preview->SetFlags(RF_Transient);

    IAnimationDataController& controller = preview->GetController();
    IAnimationDataModel* dataModel = preview->GetDataModel();
    IAnimationDataController::FScopedBracket bracket(
        controller,
        LOCTEXT("BuildHandposePreview", "Build handpose preview"),
        false);
    for (const FName boneName : boneNames)
    {
        if (!dataModel->IsValidBoneTrackName(boneName) &&
            !controller.AddBoneCurve(boneName, false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not add preview track %s."),
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
                TEXT("Could not write preview track %s."),
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
