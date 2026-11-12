#pragma once

#include "CoreMinimal.h"

class AActor;
class UAnimSequenceBase;
class USkeletalMeshComponent;
struct FHandposeTimelineSegment;

enum class EHandposeTimelineActorResult
{
    Success,
    NoActor,
    NoSkeletalMesh
};

enum class EHandposeTimelinePlaybackResult
{
    Success,
    MissingAnimationOrActor,
    IncompatibleSkeleton,
    InvalidFrameRate
};

class FHandposeTimelinePlaybackController
{
public:
    void tick();
    void setAnimation(UAnimSequenceBase* newAnimation);
    void setPlaybackRange(int32 startFrame, int32 endFrame);
    EHandposeTimelineActorResult setActor(AActor* newActor);
    EHandposeTimelinePlaybackResult playTimeline();
    EHandposeTimelinePlaybackResult previewSegment(const FHandposeTimelineSegment& segment);
    EHandposeTimelinePlaybackResult scrubTo(float timeSeconds);
    void pause();
    void toggleLoopAll();
    void toggleLoopSegment();
    float cyclePlayRate();

    AActor* getActor() const;
    float getCurrentTimeSeconds() const;
    float getPlayRate() const;
    bool isPlaying() const;
    bool shouldLoopAll() const;
    bool shouldLoopSegment() const;

private:
    EHandposeTimelinePlaybackResult validatePlayback() const;

    TWeakObjectPtr<UAnimSequenceBase> animation;
    TWeakObjectPtr<AActor> actor;
    TWeakObjectPtr<USkeletalMeshComponent> skeletalMeshComponent;
    float previewStartTimeSeconds = 0.0f;
    float previewEndTimeSeconds = 0.0f;
    float playbackStartTimeSeconds = 0.0f;
    float playbackEndTimeSeconds = 0.0f;
    float playbackLastFrameTimeSeconds = 0.0f;
    float currentTimeSeconds = 0.0f;
    float playRate = 1.0f;
    bool isPreviewingSegment = false;
    bool isPlayingTimeline = false;
    bool loopAll = false;
    bool loopSegment = false;
};
