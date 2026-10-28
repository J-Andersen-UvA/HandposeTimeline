#pragma once

#include "HandposeTimelineSegment.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class STextBlock;
class AActor;
class UAnimSequenceBase;
class USkeletalMeshComponent;

class SHandposeTimelineWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeTimelineWidget)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    virtual void Tick(
        const FGeometry& allottedGeometry,
        double currentTime,
        float deltaTime) override;

private:
    struct FTimelineBlock
    {
        int32 startFrame = INDEX_NONE;
        int32 endFrame = INDEX_NONE;
        int32 segmentIndex = INDEX_NONE;

        bool isGap() const
        {
            return segmentIndex == INDEX_NONE;
        }
    };

    FReply onLoadAnimationClicked();
    FReply onUseSelectedActorClicked();
    FReply onLoopClicked();
    FReply onSpeedClicked();
    FReply onPlayClicked();
    FReply onPauseClicked();
    FReply onSegmentClicked(int32 segmentIndex);
    void onPlayheadScrubbed(float sliderValue);
    void onZoomChanged(float sliderValue);
    void loadAnimation(UAnimSequenceBase* animation);
    void previewSegment(const FHandposeTimelineSegment& segment);
    void selectSegment(int32 segmentIndex, bool shouldPreview);
    void buildTimelineBlocks();
    void rebuildTimeline();
    float getSegmentWidth(const FHandposeTimelineSegment& segment) const;
    float getTimelineBlockWidth(const FTimelineBlock& block) const;
    float getPlayheadOffset() const;
    float getPlayheadSliderValue() const;
    float getTotalTimelineWidth() const;
    float getTimeAtTimelineOffset(float offset) const;
    FText getTimecodeText() const;
    TArray<FName> getSegmentationBoneNames(const UAnimSequenceBase& animation) const;
    TSharedRef<SWidget> createSegmentWidget(
        const FHandposeTimelineSegment& segment,
        int32 segmentIndex);
    TSharedRef<SWidget> createGapWidget(const FTimelineBlock& block) const;

    TWeakObjectPtr<UAnimSequenceBase> loadedAnimation;
    TWeakObjectPtr<AActor> previewActor;
    TWeakObjectPtr<USkeletalMeshComponent> previewSkeletalMeshComponent;
    TArray<FHandposeTimelineSegment> segments;
    TArray<FTimelineBlock> timelineBlocks;
    int32 selectedSegmentIndex = INDEX_NONE;
    float timelineZoom = 1.0f;
    float previewStartTimeSeconds = 0.0f;
    float previewEndTimeSeconds = 0.0f;
    float currentPreviewTimeSeconds = 0.0f;
    float previewPlayRate = 1.0f;
    bool isPreviewingSegment = false;
    bool isPlayingTimeline = false;
    bool shouldLoopSegmentPreview = false;
    TSharedPtr<SHorizontalBox> segmentContainer;
    TSharedPtr<STextBlock> animationNameText;
    TSharedPtr<STextBlock> actorNameText;
    TSharedPtr<STextBlock> statusText;
    TSharedPtr<STextBlock> speedText;
    TSharedPtr<STextBlock> zoomText;
};
