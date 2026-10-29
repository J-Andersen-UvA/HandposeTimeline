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
        if (loopSegment)
        {
            previewComponent->SetPosition(previewStartTimeSeconds, false);
            previewComponent->SetPlayRate(playRate);
            previewComponent->Play(false);
            currentTimeSeconds = previewStartTimeSeconds;
        }
        else
        {
            previewComponent->Stop();
            previewComponent->SetPosition(previewEndTimeSeconds, false);
            currentTimeSeconds = previewEndTimeSeconds;
            isPreviewingSegment = false;
        }
    }
    else if (isPlayingTimeline &&
        previewComponent != nullptr &&
        animation.IsValid() &&
        currentTimeSeconds >= animation->GetPlayLength())
    {
        if (loopAll)
        {
            previewComponent->SetPosition(0.0f, false);
            previewComponent->SetPlayRate(playRate);
            previewComponent->Play(false);
            currentTimeSeconds = 0.0f;
        }
        else
        {
            previewComponent->Stop();
            currentTimeSeconds = animation->GetPlayLength();
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
    currentTimeSeconds = 0.0f;
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
    if (currentTimeSeconds >= loadedAnimation->GetPlayLength())
    {
        currentTimeSeconds = 0.0f;
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

    previewStartTimeSeconds = static_cast<float>(frameRate.AsSeconds(segment.startFrame));
    previewEndTimeSeconds = FMath::Min(
        static_cast<float>(frameRate.AsSeconds(segment.endFrame + 1)),
        loadedAnimation->GetPlayLength());

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
    if (loopAll)
    {
        loopSegment = false;
    }
}

void FHandposeTimelinePlaybackController::toggleLoopSegment()
{
    loopSegment = !loopSegment;
    if (loopSegment)
    {
        loopAll = false;
    }
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

bool FHandposeTimelinePlaybackController::shouldLoopSegment() const
{
    return loopSegment;
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
