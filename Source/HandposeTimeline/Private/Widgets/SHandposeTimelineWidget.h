#pragma once

#include "HandposeTimelineSegment.h"
#include "Widgets/SCompoundWidget.h"

class FHandposeTimelinePlaybackController;
class SHandposeTimelineTrack;
class STextBlock;
class UAnimSequenceBase;

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
    FReply onLoadAnimationClicked();
    FReply onUseSelectedActorClicked();
    FReply onLoopClicked();
    FReply onSpeedClicked();
    FReply onPlayClicked();
    FReply onPauseClicked();
    void onSegmentSelected(int32 segmentIndex);
    void onPlayheadScrubbed(float timeSeconds);
    void onZoomChanged(float sliderValue);
    void loadAnimation(UAnimSequenceBase* animation);
    void selectSegment(int32 segmentIndex, bool shouldPreview);
    void refreshTimeline();
    float getCurrentTimeSeconds() const;
    FText getTimecodeText() const;
    TArray<FName> getSegmentationBoneNames(const UAnimSequenceBase& animation) const;
    TSharedPtr<FHandposeTimelinePlaybackController> playbackController;
    TWeakObjectPtr<UAnimSequenceBase> loadedAnimation;
    TArray<FHandposeTimelineSegment> segments;
    int32 selectedSegmentIndex = INDEX_NONE;
    float timelineZoom = 1.0f;
    TSharedPtr<SHandposeTimelineTrack> timelineTrack;
    TSharedPtr<STextBlock> animationNameText;
    TSharedPtr<STextBlock> actorNameText;
    TSharedPtr<STextBlock> statusText;
    TSharedPtr<STextBlock> speedText;
    TSharedPtr<STextBlock> zoomText;
};
