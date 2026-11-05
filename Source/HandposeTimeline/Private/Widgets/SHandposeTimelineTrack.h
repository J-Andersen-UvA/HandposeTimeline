#pragma once

#include "HandposeAsset.h"
#include "Timeline/HandposeTimelineLayout.h"
#include "Widgets/SHandposeAssignmentLane.h"
#include "Widgets/SHandposeTimelineRuler.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class UAnimSequenceBase;

DECLARE_DELEGATE_TwoParams(
    FOnHandposeTimelineSegmentSelected,
    EHandposeSide,
    int32)
DECLARE_DELEGATE_OneParam(FOnHandposeTimelineScrubbed, float)
DECLARE_DELEGATE_TwoParams(
    FOnHandposeTimelineAssignmentSelected,
    EHandposeSide,
    int32)
DECLARE_DELEGATE_FourParams(
    FOnHandposeTimelineAssignmentChanged,
    EHandposeSide,
    int32,
    const FHandposeAssignment&,
    bool)
DECLARE_DELEGATE_TwoParams(
    FOnHandposeTimelineAssignmentFingerEditRequested,
    EHandposeSide,
    int32)

class SHandposeTimelineTrack : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeTimelineTrack)
        : _HandSide(EHandposeSide::Right)
        , _CurrentTimeSeconds(0.0f)
    {
    }
        SLATE_ARGUMENT(EHandposeSide, HandSide)
        SLATE_ATTRIBUTE(float, CurrentTimeSeconds)
        SLATE_EVENT(FOnHandposeTimelineSegmentSelected, OnSegmentSelected)
        SLATE_EVENT(FOnHandposeTimelineScrubbed, OnScrubbed)
        SLATE_EVENT(FOnHandposeTimelineAssignmentSelected, OnAssignmentSelected)
        SLATE_EVENT(FOnHandposeTimelineAssignmentChanged, OnAssignmentChanged)
        SLATE_EVENT(
            FOnHandposeTimelineAssignmentFingerEditRequested,
            OnAssignmentFingerEditRequested)
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
    void handleAssignmentSelected(int32 assignmentIndex);
    void handleAssignmentChanged(
        int32 assignmentIndex,
        const FHandposeAssignment& assignment,
        bool isFinal);
    void handleAssignmentFingerEditRequested(int32 assignmentIndex);
    void onPlayheadScrubbed(float timeSeconds);
    void rebuild();
    TSharedRef<SWidget> createSegmentWidget(
        const FHandposeTimelineSegment& segment,
        int32 segmentIndex);
    TSharedRef<SWidget> createGapWidget(const FHandposeTimelineBlock& block) const;
    FHandposeTimelineLayout layout;
    TAttribute<float> currentTimeSeconds;
    FOnHandposeTimelineSegmentSelected onSegmentSelected;
    FOnHandposeTimelineScrubbed onScrubbed;
    FOnHandposeTimelineAssignmentSelected onAssignmentSelected;
    FOnHandposeTimelineAssignmentChanged onAssignmentChanged;
    FOnHandposeTimelineAssignmentFingerEditRequested onAssignmentFingerEditRequested;
    EHandposeSide handSide = EHandposeSide::Right;
    int32 selectedSegmentIndex = INDEX_NONE;
    TSet<int32> highlightedSegmentIndices;
    TSharedPtr<SHorizontalBox> blockContainer;
    TSharedPtr<SHandposeAssignmentLane> assignmentLane;
    TSharedPtr<SHandposeTimelineRuler> frameRuler;
    TSharedPtr<SHandposeTimelinePlayheadOverlay> playheadOverlay;
};
