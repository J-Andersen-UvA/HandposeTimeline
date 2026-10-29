#include "HandposeAssignment.h"

namespace
{
float evaluateCubicInOut(const float alpha)
{
    const float clampedAlpha = FMath::Clamp(alpha, 0.0f, 1.0f);
    return clampedAlpha < 0.5f
        ? 4.0f * clampedAlpha * clampedAlpha * clampedAlpha
        : 1.0f - FMath::Pow(-2.0f * clampedAlpha + 2.0f, 3.0f) * 0.5f;
}
}

bool FHandposeAssignment::isValid() const
{
    return !handshapeId.IsNone() &&
        !handshapeAnimation.IsNull() &&
        blendStartFrame >= 0 &&
        fullPoseStartFrame >= blendStartFrame &&
        fullPoseEndFrame >= fullPoseStartFrame &&
        blendEndFrame >= fullPoseEndFrame;
}

float FHandposeAssignment::evaluateWeight(const double frame) const
{
    if (!isValid() || frame < blendStartFrame || frame > blendEndFrame)
    {
        return 0.0f;
    }
    if (frame < fullPoseStartFrame)
    {
        const double duration = fullPoseStartFrame - blendStartFrame;
        return duration > 0.0
            ? evaluateCubicInOut(static_cast<float>((frame - blendStartFrame) / duration))
            : 1.0f;
    }
    if (frame <= fullPoseEndFrame)
    {
        return 1.0f;
    }

    const double duration = blendEndFrame - fullPoseEndFrame;
    return duration > 0.0
        ? 1.0f - evaluateCubicInOut(
            static_cast<float>((frame - fullPoseEndFrame) / duration))
        : 1.0f;
}

void FHandposeAssignment::clampToTimeline(const int32 lastFrame)
{
    blendStartFrame = FMath::Clamp(blendStartFrame, 0, lastFrame);
    fullPoseStartFrame = FMath::Clamp(
        fullPoseStartFrame,
        blendStartFrame,
        lastFrame);
    fullPoseEndFrame = FMath::Clamp(
        fullPoseEndFrame,
        fullPoseStartFrame,
        lastFrame);
    blendEndFrame = FMath::Clamp(
        blendEndFrame,
        fullPoseEndFrame,
        lastFrame);
}

void FHandposeAssignment::shiftBy(const int32 frameDelta, const int32 lastFrame)
{
    const int32 clampedDelta = FMath::Clamp(
        frameDelta,
        -blendStartFrame,
        lastFrame - blendEndFrame);
    blendStartFrame += clampedDelta;
    fullPoseStartFrame += clampedDelta;
    fullPoseEndFrame += clampedDelta;
    blendEndFrame += clampedDelta;
}
