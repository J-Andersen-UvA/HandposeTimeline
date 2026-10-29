#pragma once

#include "Timeline/HandposeTimelineLayout.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class UAnimSequenceBase;

DECLARE_DELEGATE_OneParam(FOnHandposeTimelineSegmentSelected, int32)
DECLARE_DELEGATE_OneParam(FOnHandposeTimelineScrubbed, float)

class SHandposeTimelineTrack : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeTimelineTrack)
        : _CurrentTimeSeconds(0.0f)
    {
    }
        SLATE_ATTRIBUTE(float, CurrentTimeSeconds)
        SLATE_EVENT(FOnHandposeTimelineSegmentSelected, OnSegmentSelected)
        SLATE_EVENT(FOnHandposeTimelineScrubbed, OnScrubbed)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setTimeline(
        UAnimSequenceBase* animation,
        const TArray<FHandposeTimelineSegment>& segments,
        int32 selectedSegmentIndex,
        float zoom);

private:
    FReply onSegmentClicked(int32 segmentIndex);
    void onPlayheadScrubbed(float sliderValue);
    void rebuild();
    float getPlayheadSliderValue() const;
    TSharedRef<SWidget> createSegmentWidget(
        const FHandposeTimelineSegment& segment,
        int32 segmentIndex);
    TSharedRef<SWidget> createGapWidget(const FHandposeTimelineBlock& block) const;
    TSharedRef<SWidget> createAssignmentWidget(
        const FHandposeTimelineBlock& block,
        const FHandposeTimelineSegment* segment);

    FHandposeTimelineLayout layout;
    TAttribute<float> currentTimeSeconds;
    FOnHandposeTimelineSegmentSelected onSegmentSelected;
    FOnHandposeTimelineScrubbed onScrubbed;
    int32 selectedSegmentIndex = INDEX_NONE;
    TSharedPtr<SHorizontalBox> blockContainer;
    TSharedPtr<SHorizontalBox> assignmentContainer;
};
