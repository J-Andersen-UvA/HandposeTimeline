#include "Widgets/SHandposeTimelineTrack.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeTimelineTrack"

void SHandposeTimelineTrack::Construct(const FArguments& arguments)
{
    handSide = arguments._HandSide;
    currentTimeSeconds = arguments._CurrentTimeSeconds;
    onSegmentSelected = arguments._OnSegmentSelected;
    onScrubbed = arguments._OnScrubbed;
    onAssignmentSelected = arguments._OnAssignmentSelected;
    onAssignmentChanged = arguments._OnAssignmentChanged;
    onAssignmentFingerEditRequested = arguments._OnAssignmentFingerEditRequested;
    onFaceTouchCorrectionSelected = arguments._OnFaceTouchCorrectionSelected;
    onFaceTouchCorrectionChanged = arguments._OnFaceTouchCorrectionChanged;
    onFaceTouchCorrectionEditRequested = arguments._OnFaceTouchCorrectionEditRequested;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(12.0f)
        [
            SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 5.0f)
                    [
                        SNew(STextBlock)
                        .Text(handSide == EHandposeSide::Left
                            ? LOCTEXT("LeftHandTrack", "Left Hand")
                            : LOCTEXT("RightHandTrack", "Right Hand"))
                        .Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(frameRuler, SHandposeTimelineRuler)
                        .CurrentTimeSeconds(currentTimeSeconds)
                        .OnScrubbed(this, &SHandposeTimelineTrack::onPlayheadScrubbed)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(blockContainer, SHorizontalBox)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(1.0f, 8.0f, 0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("AssignedHandshapes", "Assigned Handshapes"))
                        .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(assignmentLane, SHandposeAssignmentLane)
                        .OnAssignmentSelected(
                            this,
                            &SHandposeTimelineTrack::handleAssignmentSelected)
                        .OnFingerEditRequested(
                            this,
                            &SHandposeTimelineTrack::handleAssignmentFingerEditRequested)
                        .OnAssignmentChanged(
                            this,
                            &SHandposeTimelineTrack::handleAssignmentChanged)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(faceTouchCorrectionContainer, SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(1.0f, 8.0f, 0.0f, 4.0f)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("FaceTouchCorrections", "Face Touch Corrections"))
                            .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(faceTouchCorrectionLane, SFaceTouchCorrectionLane)
                            .OnCorrectionSelected(
                                this,
                                &SHandposeTimelineTrack::handleFaceTouchCorrectionSelected)
                            .OnCorrectionChanged(
                                this,
                                &SHandposeTimelineTrack::handleFaceTouchCorrectionChanged)
                            .OnCorrectionEditRequested(
                                this,
                                &SHandposeTimelineTrack::handleFaceTouchCorrectionEditRequested)
                        ]
                    ]
                ]
                + SOverlay::Slot()
                [
                    SAssignNew(playheadOverlay, SHandposeTimelinePlayheadOverlay)
                    .CurrentTimeSeconds(currentTimeSeconds)
                ]
        ]
    ];

    rebuild();
}

void SHandposeTimelineTrack::setTimeline(
    UAnimSequenceBase* animation,
    const TArray<FHandposeTimelineSegment>& segments,
    const TArray<FHandposeAssignment>& assignments,
    const int32 newSelectedSegmentIndex,
    const int32 selectedAssignmentIndex,
    const float zoom,
    const int32 trimStartFrame,
    const int32 trimEndFrame,
    const bool faceTouchCorrectionModeEnabled,
    const TArray<FFaceTouchCorrectionBlock>& faceTouchCorrections,
    const int32 selectedFaceTouchCorrectionIndex)
{
    selectedSegmentIndex = newSelectedSegmentIndex;
    highlightedSegmentIndices.Reset();
    if (assignments.IsValidIndex(selectedAssignmentIndex))
    {
        highlightedSegmentIndices.Append(
            assignments[selectedAssignmentIndex].coveredSegmentIndices);
        highlightedSegmentIndices.Add(
            assignments[selectedAssignmentIndex].sourceSegmentIndex);
    }
    layout.setTimeline(animation, segments, zoom);
    if (frameRuler.IsValid())
    {
        frameRuler->setTimeline(animation, segments, zoom);
    }
    if (playheadOverlay.IsValid())
    {
        playheadOverlay->setTimeline(
            animation,
            segments,
            zoom,
            trimStartFrame,
            trimEndFrame);
    }
    if (assignmentLane.IsValid())
    {
        assignmentLane->setTimeline(
            animation,
            segments,
            assignments,
            selectedAssignmentIndex,
            zoom);
    }
    if (faceTouchCorrectionContainer.IsValid())
    {
        faceTouchCorrectionContainer->SetVisibility(
            faceTouchCorrectionModeEnabled
                ? EVisibility::Visible
                : EVisibility::Collapsed);
    }
    if (faceTouchCorrectionLane.IsValid())
    {
        faceTouchCorrectionLane->setTimeline(
            animation,
            faceTouchCorrections,
            selectedFaceTouchCorrectionIndex,
            zoom);
    }
    rebuild();
}

FReply SHandposeTimelineTrack::onSegmentClicked(const int32 segmentIndex)
{
    onSegmentSelected.ExecuteIfBound(handSide, segmentIndex);
    return FReply::Handled();
}

void SHandposeTimelineTrack::handleAssignmentSelected(const int32 assignmentIndex)
{
    onAssignmentSelected.ExecuteIfBound(handSide, assignmentIndex);
}

void SHandposeTimelineTrack::handleAssignmentChanged(
    const int32 assignmentIndex,
    const FHandposeAssignment& assignment,
    const bool isFinal)
{
    onAssignmentChanged.ExecuteIfBound(
        handSide,
        assignmentIndex,
        assignment,
        isFinal);
}

void SHandposeTimelineTrack::handleAssignmentFingerEditRequested(
    const int32 assignmentIndex)
{
    onAssignmentFingerEditRequested.ExecuteIfBound(handSide, assignmentIndex);
}

void SHandposeTimelineTrack::handleFaceTouchCorrectionSelected(
    const int32 correctionIndex)
{
    onFaceTouchCorrectionSelected.ExecuteIfBound(handSide, correctionIndex);
}

void SHandposeTimelineTrack::handleFaceTouchCorrectionChanged(
    const int32 correctionIndex,
    const FFaceTouchCorrectionBlock& correction,
    const bool isFinal)
{
    onFaceTouchCorrectionChanged.ExecuteIfBound(
        handSide,
        correctionIndex,
        correction,
        isFinal);
}

void SHandposeTimelineTrack::handleFaceTouchCorrectionEditRequested(
    const int32 correctionIndex)
{
    onFaceTouchCorrectionEditRequested.ExecuteIfBound(handSide, correctionIndex);
}

void SHandposeTimelineTrack::onPlayheadScrubbed(const float timeSeconds)
{
    onScrubbed.ExecuteIfBound(timeSeconds);
}

void SHandposeTimelineTrack::rebuild()
{
    if (!blockContainer.IsValid())
    {
        return;
    }

    blockContainer->ClearChildren();
    if (layout.getBlocks().IsEmpty())
    {
        blockContainer->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("EmptyTimeline", "No segments to display"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    for (const FHandposeTimelineBlock& block : layout.getBlocks())
    {
        blockContainer->AddSlot()
        .AutoWidth()
        [
            block.isGap()
                ? createGapWidget(block)
                : createSegmentWidget(
                    layout.getSegments()[block.segmentIndex],
                    block.segmentIndex)
        ];
    }
}

TSharedRef<SWidget> SHandposeTimelineTrack::createSegmentWidget(
    const FHandposeTimelineSegment& segment,
    const int32 segmentIndex)
{
    const FHandposeTimelineBlock* block = layout.getBlocks().FindByPredicate(
        [segmentIndex](const FHandposeTimelineBlock& candidate)
        {
            return candidate.segmentIndex == segmentIndex;
        });
    const float segmentWidth = block != nullptr ? layout.getBlockWidth(*block) : 0.0f;
    const bool isSelected = selectedSegmentIndex == segmentIndex ||
        highlightedSegmentIndices.Contains(segmentIndex);

    return SNew(SBox)
        .WidthOverride(segmentWidth)
        .HeightOverride(112.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(
                isSelected
                    ? FLinearColor(0.0f, 0.45f, 0.8f, 1.0f)
                    : FLinearColor::Transparent)
            .Padding(isSelected ? 2.0f : 0.0f)
            [
                SNew(SButton)
                .ToolTipText(FText::Format(
                    LOCTEXT(
                        "SegmentTooltip",
                        "Segment {0}\nFrameRange\n{1}-{2}\n{3} seconds"),
                    FText::AsNumber(segmentIndex + 1),
                    FText::AsNumber(segment.startFrame),
                    FText::AsNumber(segment.endFrame),
                    FText::AsNumber(segment.durationSeconds)))
                .ButtonColorAndOpacity(
                    isSelected
                        ? FLinearColor(0.08f, 0.24f, 0.38f, 1.0f)
                        : FLinearColor::White)
                .OnClicked(this, &SHandposeTimelineTrack::onSegmentClicked, segmentIndex)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("SegmentLabel", "Segment {0}"),
                            FText::AsNumber(segmentIndex + 1)))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 6.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("SegmentFrames", "FrameRange\n{0}-{1}"),
                            FText::AsNumber(segment.startFrame),
                            FText::AsNumber(segment.endFrame)))
                        .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::Format(
                            LOCTEXT("SegmentDuration", "{0} seconds"),
                            FText::AsNumber(segment.durationSeconds)))
                        .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SHandposeTimelineTrack::createGapWidget(
    const FHandposeTimelineBlock& block) const
{
    return SNew(SBox)
        .WidthOverride(layout.getBlockWidth(block))
        .HeightOverride(112.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f))
            .Padding(6.0f)
            .ToolTipText(FText::Format(
                LOCTEXT("UnsegmentedFramesTooltip", "Unsegmented frames\nFrameRange\n{0}-{1}"),
                FText::AsNumber(block.startFrame),
                FText::AsNumber(block.endFrame)))
        ];
}

#undef LOCTEXT_NAMESPACE
