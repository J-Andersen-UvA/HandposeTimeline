#pragma once

#include "Timeline/HandposeTimelineLayout.h"
#include "Widgets/SHandposeAssignmentLane.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class UAnimSequenceBase;

DECLARE_DELEGATE_OneParam(FOnHandposeTimelineSegmentSelected, int32)
DECLARE_DELEGATE_OneParam(FOnHandposeTimelineScrubbed, float)
DECLARE_DELEGATE_OneParam(FOnHandposeTimelineZoomWheel, float)

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
        SLATE_EVENT(FOnHandposeTimelineZoomWheel, OnZoomWheel)
        SLATE_EVENT(FOnHandposeAssignmentSelected, OnAssignmentSelected)
        SLATE_EVENT(FOnHandposeAssignmentChanged, OnAssignmentChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setTimeline(
        UAnimSequenceBase* animation,
        const TArray<FHandposeTimelineSegment>& segments,
        const TArray<FHandposeAssignment>& assignments,
        int32 selectedSegmentIndex,
        int32 selectedAssignmentIndex,
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
    FHandposeTimelineLayout layout;
    TAttribute<float> currentTimeSeconds;
    FOnHandposeTimelineSegmentSelected onSegmentSelected;
    FOnHandposeTimelineScrubbed onScrubbed;
    FOnHandposeTimelineZoomWheel onZoomWheel;
    FOnHandposeAssignmentSelected onAssignmentSelected;
    FOnHandposeAssignmentChanged onAssignmentChanged;
    int32 selectedSegmentIndex = INDEX_NONE;
    TSharedPtr<SHorizontalBox> blockContainer;
    TSharedPtr<SHandposeAssignmentLane> assignmentLane;
};
