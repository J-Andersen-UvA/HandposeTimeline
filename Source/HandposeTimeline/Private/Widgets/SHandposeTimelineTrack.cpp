#include "Widgets/SHandposeTimelineTrack.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandposeTimelineTrack"

void SHandposeTimelineTrack::Construct(const FArguments& arguments)
{
    currentTimeSeconds = arguments._CurrentTimeSeconds;
    onSegmentSelected = arguments._OnSegmentSelected;
    onScrubbed = arguments._OnScrubbed;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(12.0f)
        [
            SNew(SScrollBox)
            .Orientation(Orient_Horizontal)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(20.0f)
                    [
                        SNew(SSlider)
                        .Value(this, &SHandposeTimelineTrack::getPlayheadSliderValue)
                        .ToolTipText(LOCTEXT("PlayheadTooltip", "Drag to scrub the animation."))
                        .OnValueChanged(this, &SHandposeTimelineTrack::onPlayheadScrubbed)
                    ]
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
                    SAssignNew(assignmentContainer, SHorizontalBox)
                ]
            ]
        ]
    ];

    rebuild();
}

void SHandposeTimelineTrack::setTimeline(
    UAnimSequenceBase* animation,
    const TArray<FHandposeTimelineSegment>& segments,
    const int32 newSelectedSegmentIndex,
    const float zoom)
{
    selectedSegmentIndex = newSelectedSegmentIndex;
    layout.setTimeline(animation, segments, zoom);
    rebuild();
}

FReply SHandposeTimelineTrack::onSegmentClicked(const int32 segmentIndex)
{
    onSegmentSelected.ExecuteIfBound(segmentIndex);
    return FReply::Handled();
}

void SHandposeTimelineTrack::onPlayheadScrubbed(const float sliderValue)
{
    onScrubbed.ExecuteIfBound(layout.getTimeAtSliderValue(sliderValue));
}

void SHandposeTimelineTrack::rebuild()
{
    if (!blockContainer.IsValid() || !assignmentContainer.IsValid())
    {
        return;
    }

    blockContainer->ClearChildren();
    assignmentContainer->ClearChildren();
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
        const FHandposeTimelineSegment* segment =
            !block.isGap() && layout.getSegments().IsValidIndex(block.segmentIndex)
                ? &layout.getSegments()[block.segmentIndex]
                : nullptr;
        blockContainer->AddSlot()
        .AutoWidth()
        .Padding(1.0f, 0.0f)
        [
            block.isGap()
                ? createGapWidget(block)
                : createSegmentWidget(
                    layout.getSegments()[block.segmentIndex],
                    block.segmentIndex)
        ];
        assignmentContainer->AddSlot()
        .AutoWidth()
        .Padding(1.0f, 0.0f)
        [
            createAssignmentWidget(block, segment)
        ];
    }
}

TSharedRef<SWidget> SHandposeTimelineTrack::createAssignmentWidget(
    const FHandposeTimelineBlock& block,
    const FHandposeTimelineSegment* segment)
{
    const float width = layout.getBlockWidth(block);
    if (block.isGap() || segment == nullptr)
    {
        return SNew(SBox)
            .WidthOverride(width)
            .HeightOverride(36.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FLinearColor(0.07f, 0.07f, 0.07f, 1.0f))
            ];
    }

    const bool isAssigned = !segment->selectedHandshapeId.IsNone();
    return SNew(SBox)
        .WidthOverride(width)
        .HeightOverride(36.0f)
        [
            SNew(SButton)
            .ButtonColorAndOpacity(
                isAssigned
                    ? FLinearColor(0.08f, 0.34f, 0.2f, 1.0f)
                    : FLinearColor(0.16f, 0.16f, 0.16f, 1.0f))
            .ToolTipText(isAssigned
                ? FText::Format(
                    LOCTEXT("AssignedHandshapeTooltip", "Assigned: {0}"),
                    FText::FromName(segment->selectedHandshapeId))
                : LOCTEXT("UnassignedHandshapeTooltip", "No handshape assigned"))
            .OnClicked(
                this,
                &SHandposeTimelineTrack::onSegmentClicked,
                block.segmentIndex)
            [
                SNew(STextBlock)
                .Text(isAssigned
                    ? FText::FromName(segment->selectedHandshapeId)
                    : LOCTEXT("Unassigned", "Unassigned"))
                .Justification(ETextJustify::Center)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
            ]
        ];
}

float SHandposeTimelineTrack::getPlayheadSliderValue() const
{
    return layout.getSliderValue(currentTimeSeconds.Get(0.0f));
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
    const bool isSelected = selectedSegmentIndex == segmentIndex;

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
