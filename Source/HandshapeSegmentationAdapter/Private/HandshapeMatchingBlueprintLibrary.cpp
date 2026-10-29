#include "HandshapeMatchingBlueprintLibrary.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "HandshapeSegmentationBridge.h"

#include <Eigen/Geometry>
#include <vector>

namespace
{
Eigen::Quaterniond toMatchingEigenQuaternion(const FQuat& quaternion)
{
    return Eigen::Quaterniond(
        quaternion.W,
        quaternion.X,
        quaternion.Y,
        quaternion.Z);
}

bool extractAveragePose(
    const UAnimSequenceBase* animation,
    const int32 averageStartFrame,
    const int32 averageEndFrame,
    const bool useMiddleHalf,
    FAnimPoseEvaluationOptions evaluationOptions,
    const FHandshapeSegmentationSettings& settings,
    std::vector<Eigen::Quaterniond>& averagePose,
    FString& errorMessage)
{
    if (animation == nullptr || animation->GetSkeleton() == nullptr)
    {
        errorMessage = TEXT("Every handshape animation must have a skeleton.");
        return false;
    }
    if (settings.boneNames.IsEmpty())
    {
        errorMessage = TEXT("At least one matching bone is required.");
        return false;
    }

    const FReferenceSkeleton& referenceSkeleton = animation->GetSkeleton()->GetReferenceSkeleton();
    for (const FName boneName : settings.boneNames)
    {
        if (boneName.IsNone() || referenceSkeleton.FindBoneIndex(boneName) == INDEX_NONE)
        {
            errorMessage = FString::Printf(
                TEXT("Animation %s does not contain matching bone %s."),
                *animation->GetName(),
                *boneName.ToString());
            return false;
        }
    }

    const int32 frameCount = animation->GetNumberOfSampledKeys();
    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    if (frameCount < 1 || !frameRate.IsValid())
    {
        errorMessage = FString::Printf(
            TEXT("Animation %s has no sampled frames or has an invalid frame rate."),
            *animation->GetName());
        return false;
    }

    TArray<double> timeIntervals;
    timeIntervals.Reserve(frameCount);
    for (int32 frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        timeIntervals.Add(frameRate.AsSeconds(frameIndex));
    }

    evaluationOptions.bEvaluateCurves = false;
    TArray<FAnimPose> poses;
    UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(
        animation,
        timeIntervals,
        evaluationOptions,
        poses);
    if (poses.Num() != frameCount)
    {
        errorMessage = FString::Printf(
            TEXT("Unreal did not return every requested pose for %s."),
            *animation->GetName());
        return false;
    }

    const int32 clampedStart = FMath::Clamp(averageStartFrame, 0, frameCount - 1);
    const int32 clampedEnd = FMath::Clamp(averageEndFrame, clampedStart, frameCount - 1);
    const int32 rangeLength = clampedEnd - clampedStart + 1;
    const int32 startFrame = useMiddleHalf
        ? clampedStart + rangeLength / 4
        : clampedStart;
    const int32 endFrame = useMiddleHalf
        ? clampedEnd - rangeLength / 4
        : clampedEnd;

    averagePose.clear();
    averagePose.reserve(settings.boneNames.Num());
    for (const FName boneName : settings.boneNames)
    {
        std::vector<Eigen::Quaterniond> rotations;
        rotations.reserve(frameCount);
        for (const FAnimPose& pose : poses)
        {
            if (!UAnimPoseExtensions::IsValid(pose))
            {
                errorMessage = FString::Printf(
                    TEXT("Unreal returned an invalid pose for %s."),
                    *animation->GetName());
                return false;
            }
            const FTransform localTransform = UAnimPoseExtensions::GetBonePose(
                pose,
                boneName,
                EAnimPoseSpaces::Local);
            rotations.push_back(toMatchingEigenQuaternion(
                localTransform.GetRotation().GetNormalized()));
        }

        std::vector<handshapeSegmentationBridge::Quaternion> bridgeRotations;
        bridgeRotations.reserve(rotations.size());
        for (const Eigen::Quaterniond& rotation : rotations)
        {
            bridgeRotations.push_back({
                rotation.w(),
                rotation.x(),
                rotation.y(),
                rotation.z()
            });
        }
        handshapeSegmentationBridge::prepareQuaternionTrack(
            bridgeRotations,
            FMath::Max(settings.quaternionSmoothingSigma, 0.01));
        for (size_t index = 0; index < rotations.size(); ++index)
        {
            rotations[index] = Eigen::Quaterniond(
                bridgeRotations[index].w,
                bridgeRotations[index].x,
                bridgeRotations[index].y,
                bridgeRotations[index].z);
        }

        Eigen::Quaterniond averageRotation(0.0, 0.0, 0.0, 0.0);
        for (int32 frameIndex = startFrame; frameIndex <= endFrame; ++frameIndex)
        {
            averageRotation.coeffs() += rotations[frameIndex].coeffs();
        }
        averageRotation.coeffs() /= static_cast<double>(endFrame - startFrame + 1);
        averageRotation.normalize();
        averagePose.push_back(averageRotation);
    }
    return true;
}

handshapeSegmentationBridge::Pose toBridgePose(
    const std::vector<Eigen::Quaterniond>& rotations)
{
    handshapeSegmentationBridge::Pose pose;
    pose.joints.reserve(rotations.size());
    for (const Eigen::Quaterniond& rotation : rotations)
    {
        pose.joints.push_back({
            rotation.w(),
            rotation.x(),
            rotation.y(),
            rotation.z()
        });
    }
    return pose;
}

handshapeSegmentationBridge::MatchingMethod toBridgeMatchingMethod(
    const EHandshapeMatchingMethod method)
{
    switch (method)
    {
    case EHandshapeMatchingMethod::Cosine:
        return handshapeSegmentationBridge::MatchingMethod::Cosine;
    case EHandshapeMatchingMethod::Manhattan:
        return handshapeSegmentationBridge::MatchingMethod::Manhattan;
    case EHandshapeMatchingMethod::Euclidean:
    default:
        return handshapeSegmentationBridge::MatchingMethod::Euclidean;
    }
}
}

FHandshapeMatchingResult UHandshapeMatchingBlueprintLibrary::matchSegmentToHandshapes(
    const UAnimSequenceBase* animation,
    const int32 startFrame,
    const int32 endFrame,
    const TArray<UAnimSequenceBase*>& handshapeAnimations,
    FAnimPoseEvaluationOptions evaluationOptions,
    const FHandshapeSegmentationSettings& settings,
    const EHandshapeMatchingMethod matchingMethod)
{
    FHandshapeMatchingResult result;
    if (animation == nullptr)
    {
        result.errorMessage = TEXT("Animation is required.");
        return result;
    }
    if (startFrame < 0 || endFrame < startFrame)
    {
        result.errorMessage = TEXT("The segment frame range is invalid.");
        return result;
    }
    if (handshapeAnimations.IsEmpty())
    {
        result.errorMessage = TEXT("At least one handshape animation is required.");
        return result;
    }

    std::vector<Eigen::Quaterniond> inputPose;
    if (!extractAveragePose(
        animation,
        startFrame,
        endFrame,
        true,
        evaluationOptions,
        settings,
        inputPose,
        result.errorMessage))
    {
        return result;
    }

    std::vector<handshapeSegmentationBridge::Pose> handshapePoses;
    TArray<UAnimSequenceBase*> validHandshapeAnimations;
    for (UAnimSequenceBase* handshapeAnimation : handshapeAnimations)
    {
        if (handshapeAnimation == nullptr)
        {
            continue;
        }

        std::vector<Eigen::Quaterniond> handshapePose;
        FString handshapeError;
        if (!extractAveragePose(
            handshapeAnimation,
            0,
            handshapeAnimation->GetNumberOfSampledKeys() - 1,
            false,
            evaluationOptions,
            settings,
            handshapePose,
            handshapeError))
        {
            continue;
        }

        handshapePoses.push_back(toBridgePose(handshapePose));
        validHandshapeAnimations.Add(handshapeAnimation);
    }

    if (handshapePoses.empty())
    {
        result.errorMessage = TEXT("No compatible handshape animations could be evaluated.");
        return result;
    }

    const std::vector<handshapeSegmentationBridge::Match> upstreamMatches =
        handshapeSegmentationBridge::matchPoses(
            handshapePoses,
            toBridgePose(inputPose),
            toBridgeMatchingMethod(matchingMethod));
    result.matches.Reserve(static_cast<int32>(upstreamMatches.size()));
    for (const handshapeSegmentationBridge::Match& upstreamMatch : upstreamMatches)
    {
        if (!validHandshapeAnimations.IsValidIndex(upstreamMatch.handshapeIndex))
        {
            continue;
        }
        UAnimSequenceBase* handshapeAnimation =
            validHandshapeAnimations[upstreamMatch.handshapeIndex];
        FHandshapeMatch& match = result.matches.AddDefaulted_GetRef();
        match.handshapeId = handshapeAnimation->GetFName();
        match.handshapeAnimation = handshapeAnimation;
        match.distance = upstreamMatch.distance;
    }

    result.success = true;
    return result;
}
