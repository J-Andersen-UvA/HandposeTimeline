#include "HandshapeMatchingBlueprintLibrary.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "HandshapeSegmentationBridge.h"

#include <Eigen/Geometry>
#include <cmath>
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

bool addDescriptorBone(
    const UAnimSequenceBase& animation,
    const TCHAR* configPath,
    const FName boneName,
    TArray<FName>& orderedBoneNames,
    FString& errorMessage)
{
    if (boneName.IsNone())
    {
        errorMessage = FString::Printf(
            TEXT("Bone mapping %s is not configured."),
            configPath);
        return false;
    }
    const FReferenceSkeleton& referenceSkeleton =
        animation.GetSkeleton()->GetReferenceSkeleton();
    if (referenceSkeleton.FindBoneIndex(boneName) == INDEX_NONE)
    {
        errorMessage = FString::Printf(
            TEXT("Animation %s does not contain %s bone %s."),
            *animation.GetName(),
            configPath,
            *boneName.ToString());
        return false;
    }
    orderedBoneNames.Add(boneName);
    return true;
}

void addOptionalTip(
    const UAnimSequenceBase& animation,
    const TCHAR* configPath,
    const FName boneName,
    TArray<FName>& orderedBoneNames,
    FString& errorMessage,
    bool& success)
{
    if (boneName.IsNone())
    {
        orderedBoneNames.Add(NAME_None);
        return;
    }
    success = addDescriptorBone(
        animation,
        configPath,
        boneName,
        orderedBoneNames,
        errorMessage);
}

bool getOrderedDescriptorBones(
    const UAnimSequenceBase& animation,
    const FHandshapeBoneMapping& mapping,
    TArray<FName>& orderedBoneNames,
    FString& errorMessage)
{
    orderedBoneNames.Reset(24);
#define ADD_BONE(path, value) \
    if (!addDescriptorBone(animation, TEXT(path), value, orderedBoneNames, errorMessage)) \
    { \
        return false; \
    }
    ADD_BONE("bones.thumb.metacarpal", mapping.thumb.metacarpal)
    ADD_BONE("bones.thumb.proximal", mapping.thumb.proximal)
    ADD_BONE("bones.thumb.distal", mapping.thumb.distal)
    bool success = true;
    addOptionalTip(
        animation,
        TEXT("bones.thumb.tip"),
        mapping.thumb.tip,
        orderedBoneNames,
        errorMessage,
        success);
    if (!success)
    {
        return false;
    }
    ADD_BONE("bones.index.metacarpal", mapping.index.metacarpal)
    ADD_BONE("bones.index.proximal", mapping.index.proximal)
    ADD_BONE("bones.index.middle", mapping.index.middle)
    ADD_BONE("bones.index.distal", mapping.index.distal)
    addOptionalTip(
        animation,
        TEXT("bones.index.tip"),
        mapping.index.tip,
        orderedBoneNames,
        errorMessage,
        success);
    if (!success)
    {
        return false;
    }
    ADD_BONE("bones.middle.metacarpal", mapping.middle.metacarpal)
    ADD_BONE("bones.middle.proximal", mapping.middle.proximal)
    ADD_BONE("bones.middle.middle", mapping.middle.middle)
    ADD_BONE("bones.middle.distal", mapping.middle.distal)
    addOptionalTip(
        animation,
        TEXT("bones.middle.tip"),
        mapping.middle.tip,
        orderedBoneNames,
        errorMessage,
        success);
    if (!success)
    {
        return false;
    }
    ADD_BONE("bones.ring.metacarpal", mapping.ring.metacarpal)
    ADD_BONE("bones.ring.proximal", mapping.ring.proximal)
    ADD_BONE("bones.ring.middle", mapping.ring.middle)
    ADD_BONE("bones.ring.distal", mapping.ring.distal)
    addOptionalTip(
        animation,
        TEXT("bones.ring.tip"),
        mapping.ring.tip,
        orderedBoneNames,
        errorMessage,
        success);
    if (!success)
    {
        return false;
    }
    ADD_BONE("bones.pinky.metacarpal", mapping.pinky.metacarpal)
    ADD_BONE("bones.pinky.proximal", mapping.pinky.proximal)
    ADD_BONE("bones.pinky.middle", mapping.pinky.middle)
    ADD_BONE("bones.pinky.distal", mapping.pinky.distal)
    addOptionalTip(
        animation,
        TEXT("bones.pinky.tip"),
        mapping.pinky.tip,
        orderedBoneNames,
        errorMessage,
        success);
    if (!success)
    {
        return false;
    }
#undef ADD_BONE
    return orderedBoneNames.Num() == 24;
}

bool extractHandSkeletonTrack(
    const UAnimSequenceBase* animation,
    FAnimPoseEvaluationOptions evaluationOptions,
    const FHandshapeBoneMapping& boneMapping,
    const bool useVirtualLeafBones,
    const double virtualLeafBoneExtensionCm,
    handshapeSegmentationBridge::HandSkeletonTrack& track,
    FString& errorMessage)
{
    if (animation == nullptr || animation->GetSkeleton() == nullptr)
    {
        errorMessage = TEXT("Every handshape animation must have a skeleton.");
        return false;
    }

    TArray<FName> orderedBoneNames;
    if (!getOrderedDescriptorBones(
        *animation,
        boneMapping,
        orderedBoneNames,
        errorMessage))
    {
        return false;
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

    track.frames.clear();
    track.frames.reserve(frameCount);
    for (const FAnimPose& pose : poses)
    {
        if (!UAnimPoseExtensions::IsValid(pose))
        {
            errorMessage = FString::Printf(
                TEXT("Unreal returned an invalid pose for %s."),
                *animation->GetName());
            return false;
        }
        std::vector<handshapeSegmentationBridge::Position>& frame =
            track.frames.emplace_back();
        frame.reserve(orderedBoneNames.Num());
        for (const FName boneName : orderedBoneNames)
        {
            const FVector position = boneName.IsNone()
                ? FVector::ZeroVector
                : UAnimPoseExtensions::GetBonePose(
                    pose,
                    boneName,
                    EAnimPoseSpaces::World).GetLocation();
            frame.push_back({position.X, position.Y, position.Z});
        }
        static const int32 tipIndices[] = {3, 8, 13, 18, 23};
        static const int32 precedingJointIndices[] = {1, 6, 11, 16, 21};
        const double extensionCm = FMath::Clamp(
            virtualLeafBoneExtensionCm,
            1.0,
            3.0);
        for (int32 fingerIndex = 0; fingerIndex < 5; ++fingerIndex)
        {
            const int32 tipIndex = tipIndices[fingerIndex];
            handshapeSegmentationBridge::Position& tip = frame[tipIndex];
            const handshapeSegmentationBridge::Position& distal =
                frame[tipIndex - 1];
            const bool hasConfiguredTip =
                !orderedBoneNames[tipIndex].IsNone();
            if (!hasConfiguredTip)
            {
                tip = distal;
            }
            if (!useVirtualLeafBones && hasConfiguredTip)
            {
                continue;
            }
            const handshapeSegmentationBridge::Position& directionStart =
                hasConfiguredTip
                    ? distal
                    : frame[precedingJointIndices[fingerIndex]];
            const double directionX = tip.x - directionStart.x;
            const double directionY = tip.y - directionStart.y;
            const double directionZ = tip.z - directionStart.z;
            const double directionLength = std::sqrt(
                directionX * directionX +
                directionY * directionY +
                directionZ * directionZ);
            if (directionLength > UE_DOUBLE_SMALL_NUMBER)
            {
                tip.x += directionX / directionLength * extensionCm;
                tip.y += directionY / directionLength * extensionCm;
                tip.z += directionZ / directionLength * extensionCm;
            }
        }
    }
    return true;
}

FHandshapeMatchingResult matchUsingHandSkeletonDescriptor(
    const UAnimSequenceBase* animation,
    const int32 startFrame,
    const int32 endFrame,
    const TArray<UAnimSequenceBase*>& handshapeAnimations,
    FAnimPoseEvaluationOptions evaluationOptions,
    const EHandshapeMatchingMethod matchingMethod,
    const FHandshapeBoneMapping& boneMapping,
    const bool useVirtualLeafBones,
    const double virtualLeafBoneExtensionCm)
{
    FHandshapeMatchingResult result;
    handshapeSegmentationBridge::HandSkeletonTrack inputTrack;
    if (!extractHandSkeletonTrack(
        animation,
        evaluationOptions,
        boneMapping,
        useVirtualLeafBones,
        virtualLeafBoneExtensionCm,
        inputTrack,
        result.errorMessage))
    {
        return result;
    }

    std::vector<handshapeSegmentationBridge::HandSkeletonTrack> handshapeTracks;
    TArray<UAnimSequenceBase*> validHandshapeAnimations;
    for (UAnimSequenceBase* handshapeAnimation : handshapeAnimations)
    {
        handshapeSegmentationBridge::HandSkeletonTrack handshapeTrack;
        FString handshapeError;
        if (extractHandSkeletonTrack(
            handshapeAnimation,
            evaluationOptions,
            boneMapping,
            useVirtualLeafBones,
            virtualLeafBoneExtensionCm,
            handshapeTrack,
            handshapeError))
        {
            handshapeTracks.push_back(MoveTemp(handshapeTrack));
            validHandshapeAnimations.Add(handshapeAnimation);
        }
    }
    if (handshapeTracks.empty())
    {
        result.errorMessage = TEXT(
            "No compatible handshape animations contain the configured descriptor bones.");
        return result;
    }

    const int32 lastInputFrame = static_cast<int32>(inputTrack.frames.size()) - 1;
    handshapeSegmentationBridge::HandSkeletonMatchDiagnostics diagnostics;
    const std::vector<handshapeSegmentationBridge::Match> upstreamMatches =
        handshapeSegmentationBridge::matchHandSkeletonTracks(
            handshapeTracks,
            inputTrack,
            FMath::Clamp(startFrame, 0, lastInputFrame),
            FMath::Clamp(endFrame, 0, lastInputFrame),
            toBridgeMatchingMethod(matchingMethod),
            &diagnostics);
    result.inputDescriptor.Reserve(
        static_cast<int32>(diagnostics.inputDescriptor.size()));
    for (const double value : diagnostics.inputDescriptor)
    {
        result.inputDescriptor.Add(value);
    }
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
        if (diagnostics.handshapeDescriptors.size() >
            static_cast<size_t>(upstreamMatch.handshapeIndex))
        {
            const std::vector<double>& descriptor =
                diagnostics.handshapeDescriptors[upstreamMatch.handshapeIndex];
            match.descriptor.Reserve(static_cast<int32>(descriptor.size()));
            for (const double value : descriptor)
            {
                match.descriptor.Add(value);
            }
        }
    }
    result.success = true;
    return result;
}
}

FHandshapeMatchingResult UHandshapeMatchingBlueprintLibrary::matchSegmentToHandshapes(
    const UAnimSequenceBase* animation,
    const int32 startFrame,
    const int32 endFrame,
    const TArray<UAnimSequenceBase*>& handshapeAnimations,
    FAnimPoseEvaluationOptions evaluationOptions,
    const FHandshapeSegmentationSettings& settings,
    const EHandshapeMatchingMethod matchingMethod,
    const EHandshapeDescriptorMethod descriptorMethod,
    const FHandshapeBoneMapping& boneMapping,
    const bool useVirtualLeafBones,
    const double virtualLeafBoneExtensionCm)
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
    if (descriptorMethod == EHandshapeDescriptorMethod::HandSkeleton)
    {
        return matchUsingHandSkeletonDescriptor(
            animation,
            startFrame,
            endFrame,
            handshapeAnimations,
            evaluationOptions,
            matchingMethod,
            boneMapping,
            useVirtualLeafBones,
            virtualLeafBoneExtensionCm);
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
