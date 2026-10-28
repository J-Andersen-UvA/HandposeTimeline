#pragma once

#include "CoreMinimal.h"
#include "HandposeTimelineSegment.h"

class UAnimSequenceBase;

struct FHandposeTimelineBlock
{
    int32 startFrame = INDEX_NONE;
    int32 endFrame = INDEX_NONE;
    int32 segmentIndex = INDEX_NONE;

    bool isGap() const
    {
        return segmentIndex == INDEX_NONE;
    }
};

class FHandposeTimelineLayout
{
public:
    void setTimeline(
        UAnimSequenceBase* animation,
        const TArray<FHandposeTimelineSegment>& newSegments,
        float newZoom);

    const TArray<FHandposeTimelineBlock>& getBlocks() const;
    const TArray<FHandposeTimelineSegment>& getSegments() const;
    float getBlockWidth(const FHandposeTimelineBlock& block) const;
    float getTotalWidth() const;
    float getSliderValue(float timeSeconds) const;
    float getTimeAtSliderValue(float sliderValue) const;

private:
    void buildBlocks();
    float getOffsetAtTime(float timeSeconds) const;

    TWeakObjectPtr<UAnimSequenceBase> animation;
    TArray<FHandposeTimelineSegment> segments;
    TArray<FHandposeTimelineBlock> blocks;
    float zoom = 1.0f;
};
