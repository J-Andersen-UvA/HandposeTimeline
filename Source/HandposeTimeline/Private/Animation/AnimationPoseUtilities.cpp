#include "Animation/AnimationPoseUtilities.h"

#include "AnimPose.h"
#include "Animation/AnimSequence.h"

namespace
{
FAnimPoseEvaluationOptions makeEvaluationOptions()
{
    FAnimPoseEvaluationOptions options;
    options.EvaluationType = EAnimDataEvalType::Raw;
    options.bShouldRetarget = true;
    options.bEvaluateCurves = false;
    return options;
}
}

bool HandposeTimeline::AnimationPoseUtilities::evaluatePoses(
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

UAnimSequence* HandposeTimeline::AnimationPoseUtilities::duplicateTransientAnimation(
    UAnimSequence& sourceAnimation,
    const FName baseName)
{
    UAnimSequence* animation = DuplicateObject<UAnimSequence>(
        &sourceAnimation,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UAnimSequence::StaticClass(),
            baseName));
    if (animation != nullptr)
    {
        animation->ClearFlags(RF_Public | RF_Standalone);
        animation->SetFlags(RF_Transient);
    }
    return animation;
}
