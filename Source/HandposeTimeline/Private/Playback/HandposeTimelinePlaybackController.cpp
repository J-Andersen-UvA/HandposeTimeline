#include "Playback/HandposeTimelinePlaybackController.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "HandposeTimelineSegment.h"

void FHandposeTimelinePlaybackController::tick()
{
    USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get();
    if ((isPreviewingSegment || isPlayingTimeline) && previewComponent != nullptr)
    {
        currentTimeSeconds = previewComponent->GetPosition();
    }

    if (isPreviewingSegment &&
        previewComponent != nullptr &&
        currentTimeSeconds >= previewEndTimeSeconds)
    {
        previewComponent->Stop();
        previewComponent->SetPosition(previewEndTimeSeconds, false);
        currentTimeSeconds = previewEndTimeSeconds;
        isPreviewingSegment = false;
    }
    else if (isPlayingTimeline &&
        previewComponent != nullptr &&
        animation.IsValid() &&
        currentTimeSeconds >= playbackEndTimeSeconds)
    {
        if (loopAll)
        {
            previewComponent->SetPosition(playbackStartTimeSeconds, false);
            previewComponent->SetPlayRate(playRate);
            previewComponent->Play(false);
            currentTimeSeconds = playbackStartTimeSeconds;
        }
        else
        {
            previewComponent->Stop();
            previewComponent->SetPosition(playbackLastFrameTimeSeconds, false);
            currentTimeSeconds = playbackLastFrameTimeSeconds;
            isPlayingTimeline = false;
        }
    }
}

void FHandposeTimelinePlaybackController::setAnimation(UAnimSequenceBase* newAnimation)
{
    pause();
    animation = newAnimation;
    previewStartTimeSeconds = 0.0f;
    previewEndTimeSeconds = 0.0f;
    playbackStartTimeSeconds = 0.0f;
    playbackEndTimeSeconds = newAnimation != nullptr
        ? newAnimation->GetPlayLength()
        : 0.0f;
    playbackLastFrameTimeSeconds = playbackEndTimeSeconds;
    currentTimeSeconds = 0.0f;
}

void FHandposeTimelinePlaybackController::setPlaybackRange(
    const int32 startFrame,
    const int32 endFrame)
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        playbackStartTimeSeconds = 0.0f;
        playbackEndTimeSeconds = loadedAnimation != nullptr
            ? loadedAnimation->GetPlayLength()
            : 0.0f;
        playbackLastFrameTimeSeconds = playbackEndTimeSeconds;
        return;
    }

    const int32 lastFrame = FMath::Max(0, loadedAnimation->GetNumberOfSampledKeys() - 1);
    const int32 clampedStartFrame = FMath::Clamp(startFrame, 0, lastFrame);
    const int32 clampedEndFrame = FMath::Clamp(endFrame, clampedStartFrame, lastFrame);
    const FFrameRate frameRate = loadedAnimation->GetSamplingFrameRate();
    playbackStartTimeSeconds = static_cast<float>(frameRate.AsSeconds(clampedStartFrame));
    playbackLastFrameTimeSeconds = static_cast<float>(frameRate.AsSeconds(clampedEndFrame));
    playbackEndTimeSeconds = FMath::Min(
        static_cast<float>(frameRate.AsSeconds(clampedEndFrame + 1)),
        loadedAnimation->GetPlayLength());
}

EHandposeTimelineActorResult FHandposeTimelinePlaybackController::setActor(AActor* newActor)
{
    pause();
    actor.Reset();
    skeletalMeshComponent.Reset();

    if (newActor == nullptr)
    {
        return EHandposeTimelineActorResult::NoActor;
    }

    USkeletalMeshComponent* newSkeletalMeshComponent =
        newActor->FindComponentByClass<USkeletalMeshComponent>();
    if (newSkeletalMeshComponent == nullptr)
    {
        return EHandposeTimelineActorResult::NoSkeletalMesh;
    }

    actor = newActor;
    skeletalMeshComponent = newSkeletalMeshComponent;
    return EHandposeTimelineActorResult::Success;
}

EHandposeTimelinePlaybackResult FHandposeTimelinePlaybackController::playTimeline()
{
    const EHandposeTimelinePlaybackResult validationResult = validatePlayback();
    if (validationResult != EHandposeTimelinePlaybackResult::Success)
    {
        return validationResult;
    }

    UAnimSequenceBase* loadedAnimation = animation.Get();
    USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get();
    if (currentTimeSeconds < playbackStartTimeSeconds ||
        currentTimeSeconds >= playbackEndTimeSeconds)
    {
        currentTimeSeconds = playbackStartTimeSeconds;
    }

    previewComponent->SetUpdateAnimationInEditor(true);
    previewComponent->PlayAnimation(loadedAnimation, false);
    previewComponent->SetPosition(currentTimeSeconds, false);
    previewComponent->SetPlayRate(playRate);
    isPreviewingSegment = false;
    isPlayingTimeline = true;
    return EHandposeTimelinePlaybackResult::Success;
}

EHandposeTimelinePlaybackResult FHandposeTimelinePlaybackController::previewSegment(
    const FHandposeTimelineSegment& segment)
{
    const EHandposeTimelinePlaybackResult validationResult = validatePlayback();
    if (validationResult != EHandposeTimelinePlaybackResult::Success)
    {
        return validationResult;
    }

    UAnimSequenceBase* loadedAnimation = animation.Get();
    const FFrameRate frameRate = loadedAnimation->GetSamplingFrameRate();
    if (!frameRate.IsValid())
    {
        return EHandposeTimelinePlaybackResult::InvalidFrameRate;
    }

    previewStartTimeSeconds = FMath::Max(
        static_cast<float>(frameRate.AsSeconds(segment.startFrame)),
        playbackStartTimeSeconds);
    previewEndTimeSeconds = FMath::Min(
        static_cast<float>(frameRate.AsSeconds(segment.endFrame + 1)),
        playbackEndTimeSeconds);
    if (previewStartTimeSeconds >= previewEndTimeSeconds)
    {
        currentTimeSeconds = playbackStartTimeSeconds;
        return EHandposeTimelinePlaybackResult::Success;
    }

    USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get();
    previewComponent->SetUpdateAnimationInEditor(true);
    previewComponent->PlayAnimation(loadedAnimation, false);
    previewComponent->SetPosition(previewStartTimeSeconds, false);
    previewComponent->SetPlayRate(playRate);
    currentTimeSeconds = previewStartTimeSeconds;
    isPreviewingSegment = true;
    isPlayingTimeline = false;
    return EHandposeTimelinePlaybackResult::Success;
}

EHandposeTimelinePlaybackResult FHandposeTimelinePlaybackController::scrubTo(
    const float timeSeconds)
{
    currentTimeSeconds = timeSeconds;
    isPreviewingSegment = false;
    isPlayingTimeline = false;

    const EHandposeTimelinePlaybackResult validationResult = validatePlayback();
    if (validationResult != EHandposeTimelinePlaybackResult::Success)
    {
        return validationResult;
    }

    UAnimSequenceBase* loadedAnimation = animation.Get();
    USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get();
    previewComponent->SetUpdateAnimationInEditor(true);
    previewComponent->PlayAnimation(loadedAnimation, false);
    previewComponent->Stop();
    previewComponent->SetPosition(currentTimeSeconds, false);
    previewComponent->TickAnimation(0.0f, false);
    previewComponent->RefreshBoneTransforms(nullptr);
    previewComponent->UpdateBounds();
    previewComponent->MarkRenderDynamicDataDirty();
    if (GEditor != nullptr)
    {
        GEditor->RedrawLevelEditingViewports(false);
    }
    return EHandposeTimelinePlaybackResult::Success;
}

void FHandposeTimelinePlaybackController::pause()
{
    if (USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get())
    {
        currentTimeSeconds = previewComponent->GetPosition();
        previewComponent->Stop();
    }
    isPreviewingSegment = false;
    isPlayingTimeline = false;
}

void FHandposeTimelinePlaybackController::toggleLoopAll()
{
    loopAll = !loopAll;
}

float FHandposeTimelinePlaybackController::cyclePlayRate()
{
    if (FMath::IsNearlyEqual(playRate, 1.0f))
    {
        playRate = 0.25f;
    }
    else if (FMath::IsNearlyEqual(playRate, 0.25f))
    {
        playRate = 0.5f;
    }
    else
    {
        playRate = 1.0f;
    }

    if (USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get())
    {
        previewComponent->SetPlayRate(playRate);
    }
    return playRate;
}

AActor* FHandposeTimelinePlaybackController::getActor() const
{
    return actor.Get();
}

float FHandposeTimelinePlaybackController::getCurrentTimeSeconds() const
{
    return currentTimeSeconds;
}

float FHandposeTimelinePlaybackController::getPlayRate() const
{
    return playRate;
}

bool FHandposeTimelinePlaybackController::isPlaying() const
{
    return isPreviewingSegment || isPlayingTimeline;
}

bool FHandposeTimelinePlaybackController::shouldLoopAll() const
{
    return loopAll;
}

EHandposeTimelinePlaybackResult FHandposeTimelinePlaybackController::validatePlayback() const
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    USkeletalMeshComponent* previewComponent = skeletalMeshComponent.Get();
    if (loadedAnimation == nullptr || previewComponent == nullptr)
    {
        return EHandposeTimelinePlaybackResult::MissingAnimationOrActor;
    }

    const USkeletalMesh* skeletalMesh = previewComponent->GetSkeletalMeshAsset();
    if (skeletalMesh == nullptr ||
        loadedAnimation->GetSkeleton() == nullptr ||
        !loadedAnimation->GetSkeleton()->IsCompatibleForEditor(skeletalMesh->GetSkeleton()))
    {
        return EHandposeTimelinePlaybackResult::IncompatibleSkeleton;
    }

    return EHandposeTimelinePlaybackResult::Success;
}
