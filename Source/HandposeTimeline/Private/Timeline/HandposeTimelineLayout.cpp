#include "Timeline/HandposeTimelineLayout.h"

#include "Animation/AnimSequenceBase.h"

namespace
{
constexpr float pixelsPerSecond = 180.0f;
constexpr float minimumBlockWidth = 1.0f;
}

void FHandposeTimelineLayout::setTimeline(
    UAnimSequenceBase* newAnimation,
    const TArray<FHandposeTimelineSegment>& newSegments,
    const float newZoom)
{
    animation = newAnimation;
    segments = newSegments;
    zoom = newZoom;
    buildBlocks();
}

const TArray<FHandposeTimelineBlock>& FHandposeTimelineLayout::getBlocks() const
{
    return blocks;
}

const TArray<FHandposeTimelineSegment>& FHandposeTimelineLayout::getSegments() const
{
    return segments;
}

float FHandposeTimelineLayout::getBlockWidth(const FHandposeTimelineBlock& block) const
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        return minimumBlockWidth;
    }

    const double durationSeconds = loadedAnimation->GetSamplingFrameRate().AsSeconds(
        block.endFrame - block.startFrame + 1);
    return FMath::Max(
        minimumBlockWidth,
        static_cast<float>(durationSeconds) * pixelsPerSecond * zoom);
}

float FHandposeTimelineLayout::getTotalWidth() const
{
    float width = 0.0f;
    for (const FHandposeTimelineBlock& block : blocks)
    {
        width += getBlockWidth(block);
    }
    return width;
}

float FHandposeTimelineLayout::getOffsetAtFrame(const double frame) const
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        return 0.0f;
    }
    return getOffsetAtTime(static_cast<float>(
        frame / loadedAnimation->GetSamplingFrameRate().AsDecimal()));
}

double FHandposeTimelineLayout::getFrameAtOffset(const float offset) const
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    const float totalWidth = getTotalWidth();
    if (loadedAnimation == nullptr ||
        !loadedAnimation->GetSamplingFrameRate().IsValid() ||
        totalWidth <= 0.0f)
    {
        return 0.0;
    }
    const float timeSeconds = getTimeAtSliderValue(
        FMath::Clamp(offset / totalWidth, 0.0f, 1.0f));
    return timeSeconds * loadedAnimation->GetSamplingFrameRate().AsDecimal();
}

int32 FHandposeTimelineLayout::getLastFrame() const
{
    const UAnimSequenceBase* loadedAnimation = animation.Get();
    return loadedAnimation != nullptr
        ? FMath::Max(0, loadedAnimation->GetNumberOfSampledKeys() - 1)
        : 0;
}

float FHandposeTimelineLayout::getSliderValue(const float timeSeconds) const
{
    const float totalWidth = getTotalWidth();
    return totalWidth > 0.0f
        ? FMath::Clamp(getOffsetAtTime(timeSeconds) / totalWidth, 0.0f, 1.0f)
        : 0.0f;
}

float FHandposeTimelineLayout::getTimeAtSliderValue(const float sliderValue) const
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        return 0.0f;
    }

    const FFrameRate frameRate = loadedAnimation->GetSamplingFrameRate();
    const float targetOffset = FMath::Clamp(sliderValue, 0.0f, 1.0f) * getTotalWidth();
    float blockOffset = 0.0f;
    for (const FHandposeTimelineBlock& block : blocks)
    {
        const float blockWidth = getBlockWidth(block);
        if (targetOffset <= blockOffset + blockWidth)
        {
            const float alpha = FMath::Clamp(
                (targetOffset - blockOffset) / blockWidth,
                0.0f,
                1.0f);
            const float startTime = static_cast<float>(frameRate.AsSeconds(block.startFrame));
            const float endTime = static_cast<float>(frameRate.AsSeconds(block.endFrame + 1));
            return FMath::Lerp(startTime, endTime, alpha);
        }
        blockOffset += blockWidth;
    }

    return loadedAnimation->GetPlayLength();
}

void FHandposeTimelineLayout::buildBlocks()
{
    blocks.Reset();

    UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr)
    {
        return;
    }

    const int32 lastAnimationFrame = loadedAnimation->GetNumberOfSampledKeys() - 1;
    if (lastAnimationFrame < 0)
    {
        return;
    }

    int32 nextFrame = 0;
    for (int32 segmentIndex = 0; segmentIndex < segments.Num(); ++segmentIndex)
    {
        const FHandposeTimelineSegment& segment = segments[segmentIndex];
        const int32 segmentStart = FMath::Clamp(segment.startFrame, 0, lastAnimationFrame);
        const int32 segmentEnd = FMath::Clamp(segment.endFrame, segmentStart, lastAnimationFrame);

        if (segmentStart > nextFrame)
        {
            blocks.Add({nextFrame, segmentStart - 1, INDEX_NONE});
        }

        blocks.Add({segmentStart, segmentEnd, segmentIndex});
        nextFrame = FMath::Max(nextFrame, segmentEnd + 1);
    }

    if (nextFrame <= lastAnimationFrame)
    {
        blocks.Add({nextFrame, lastAnimationFrame, INDEX_NONE});
    }
}

float FHandposeTimelineLayout::getOffsetAtTime(const float timeSeconds) const
{
    UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        return 0.0f;
    }

    const FFrameRate frameRate = loadedAnimation->GetSamplingFrameRate();
    float offset = 0.0f;
    for (const FHandposeTimelineBlock& block : blocks)
    {
        const float blockWidth = getBlockWidth(block);
        const float startTime = static_cast<float>(frameRate.AsSeconds(block.startFrame));
        const float endTime = static_cast<float>(frameRate.AsSeconds(block.endFrame + 1));
        if (timeSeconds <= endTime)
        {
            const float duration = FMath::Max(endTime - startTime, UE_SMALL_NUMBER);
            const float alpha = FMath::Clamp((timeSeconds - startTime) / duration, 0.0f, 1.0f);
            return offset + alpha * blockWidth;
        }
        offset += blockWidth;
    }
    return offset;
}
