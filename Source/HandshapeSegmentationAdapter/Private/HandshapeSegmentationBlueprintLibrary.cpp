#include "HandshapeSegmentationBlueprintLibrary.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "HandshapeSegmentationBridge.h"

#include <utility>
#include <vector>

FHandshapeSegmentationResult
UHandshapeSegmentationBlueprintLibrary::segmentAnimation(
    const UAnimSequenceBase* animation,
    FAnimPoseEvaluationOptions evaluationOptions,
    const FHandshapeSegmentationSettings& settings)
{
    FHandshapeSegmentationResult result;
    if (animation == nullptr)
    {
        result.errorMessage = TEXT("Animation is required.");
        return result;
    }
    if (animation->GetSkeleton() == nullptr)
    {
        result.errorMessage = TEXT("Animation has no skeleton.");
        return result;
    }
    if (settings.boneNames.IsEmpty())
    {
        result.errorMessage = TEXT("At least one segmentation bone is required.");
        return result;
    }

    const FReferenceSkeleton& referenceSkeleton =
        evaluationOptions.OptionalSkeletalMesh != nullptr
            ? evaluationOptions.OptionalSkeletalMesh->GetRefSkeleton()
            : animation->GetSkeleton()->GetReferenceSkeleton();
    TArray<FName> validBoneNames;
    for (const FName boneName : settings.boneNames)
    {
        if (!boneName.IsNone() &&
            referenceSkeleton.FindBoneIndex(boneName) != INDEX_NONE &&
            !validBoneNames.Contains(boneName))
        {
            validBoneNames.Add(boneName);
        }
    }
    if (validBoneNames.IsEmpty())
    {
        result.errorMessage = TEXT("None of the segmentation bones exist on the animation skeleton.");
        return result;
    }

    const int32 frameCount = animation->GetNumberOfSampledKeys();
    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    if (frameCount < 3 || !frameRate.IsValid())
    {
        result.errorMessage = TEXT("Animation must contain at least three sampled frames and a valid frame rate.");
        return result;
    }

    result.frameCount = frameCount;
    result.frameRateNumerator = frameRate.Numerator;
    result.frameRateDenominator = frameRate.Denominator;
    result.quaternionTracks.Reserve(validBoneNames.Num());
    for (const FName boneName : validBoneNames)
    {
        FHandshapeQuaternionTrack& track =
            result.quaternionTracks.AddDefaulted_GetRef();
        track.boneName = boneName;
        track.localRotations.Reserve(frameCount);
    }

    evaluationOptions.bEvaluateCurves = false;
    constexpr int32 poseBatchSize = 256;
    for (int32 batchStart = 0; batchStart < frameCount; batchStart += poseBatchSize)
    {
        const int32 batchEnd = FMath::Min(batchStart + poseBatchSize, frameCount);
        TArray<double> timeIntervals;
        timeIntervals.Reserve(batchEnd - batchStart);
        for (int32 frameIndex = batchStart; frameIndex < batchEnd; ++frameIndex)
        {
            timeIntervals.Add(frameRate.AsSeconds(frameIndex));
        }

        TArray<FAnimPose> poses;
        UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(
            animation,
            timeIntervals,
            evaluationOptions,
            poses);
        if (poses.Num() != timeIntervals.Num())
        {
            result.quaternionTracks.Reset();
            result.errorMessage = TEXT("Unreal did not return every requested animation pose.");
            return result;
        }

        for (const FAnimPose& pose : poses)
        {
            if (!UAnimPoseExtensions::IsValid(pose))
            {
                result.quaternionTracks.Reset();
                result.errorMessage = TEXT("Unreal returned an invalid animation pose.");
                return result;
            }
            for (int32 boneIndex = 0; boneIndex < validBoneNames.Num(); ++boneIndex)
            {
                const FTransform localTransform = UAnimPoseExtensions::GetBonePose(
                    pose,
                    validBoneNames[boneIndex],
                    EAnimPoseSpaces::Local);
                result.quaternionTracks[boneIndex].localRotations.Add(
                    localTransform.GetRotation().GetNormalized());
            }
        }
    }

    std::vector<std::vector<handshapeSegmentationBridge::Quaternion>> quaternionTracks;
    quaternionTracks.reserve(result.quaternionTracks.Num());
    for (const FHandshapeQuaternionTrack& unrealTrack : result.quaternionTracks)
    {
        std::vector<handshapeSegmentationBridge::Quaternion> track;
        track.reserve(unrealTrack.localRotations.Num());
        for (const FQuat& quaternion : unrealTrack.localRotations)
        {
            track.push_back({
                quaternion.W,
                quaternion.X,
                quaternion.Y,
                quaternion.Z
            });
        }
        handshapeSegmentationBridge::prepareQuaternionTrack(
            track,
            FMath::Max(settings.quaternionSmoothingSigma, 0.01));
        quaternionTracks.push_back(std::move(track));
    }

    std::vector<double> acceleration =
        handshapeSegmentationBridge::aggregateQuaternionAcceleration(
            quaternionTracks,
            1.0 / frameRate.AsDecimal());
    handshapeSegmentationBridge::smoothValues(
        acceleration,
        FMath::Max(settings.accelerationSmoothingSigma, 0.01));
    const std::vector<handshapeSegmentationBridge::Segment> segments =
        handshapeSegmentationBridge::segmentByAcceleration(
            acceleration,
            settings.thresholdFactor,
            FMath::Max(settings.minimumSegmentLength, 1),
            settings.minimumAccelerationThreshold);

    result.segments.Reserve(static_cast<int32>(segments.size()));
    for (const handshapeSegmentationBridge::Segment& sourceSegment : segments)
    {
        FHandshapeSegment& segment = result.segments.AddDefaulted_GetRef();
        segment.startFrame = sourceSegment.start;
        segment.endFrame = sourceSegment.end;
        segment.startTimeSeconds = frameRate.AsSeconds(sourceSegment.start);
        segment.endTimeSeconds = frameRate.AsSeconds(sourceSegment.end);
    }
    result.success = true;
    return result;
}
