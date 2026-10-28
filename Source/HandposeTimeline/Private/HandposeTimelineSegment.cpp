#include "HandposeTimelineSegment.h"

bool FHandposeTimelineSegment::isValid() const
{
    return startFrame >= 0 && endFrame >= startFrame && durationSeconds >= 0.0;
}

int32 FHandposeTimelineSegment::getFrameCount() const
{
    return isValid() ? endFrame - startFrame + 1 : 0;
}

bool FHandposeTimelineSegment::hasSelectedHandshape() const
{
    return !selectedHandshapeId.IsNone();
}
