#pragma once

#include "HandshapeCandidate.h"
#include "HandposeTimelineSegment.h"
#include "Widgets/SCompoundWidget.h"

class FHandposeTimelinePlaybackController;
class SHandposeTimelineTrack;
class STextBlock;
class SWindow;
class UAnimSequenceBase;
class UHandposeTimelineConfig;

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
    virtual FReply OnPreviewKeyDown(
        const FGeometry& myGeometry,
        const FKeyEvent& keyEvent) override;

private:
    FReply onLoadAnimationClicked();
    FReply onLoadConfigClicked();
    FReply onUseSelectedActorClicked();
    FReply onLoopAllClicked();
    FReply onLoopSegmentClicked();
    FReply onSpeedClicked();
    FReply onPlayPauseClicked();
    void onSegmentSelected(int32 segmentIndex);
    void onHandshapeChosen(int32 segmentIndex, FName handshapeId);
    void onPlayheadScrubbed(float timeSeconds);
    void stepFrame(int32 frameOffset);
    void onZoomChanged(float sliderValue);
    void loadAnimation(UAnimSequenceBase* animation);
    void loadConfig(UHandposeTimelineConfig* config);
    void selectSegment(int32 segmentIndex, bool shouldPreview);
    void openHandshapePicker(int32 segmentIndex);
    TArray<FHandshapeCandidate> getCandidatesForSegment(
        const FHandposeTimelineSegment& segment,
        FString& errorMessage) const;
    void refreshTimeline();
    float getCurrentTimeSeconds() const;
    FText getPlayPauseButtonText() const;
    FText getTimecodeText() const;
    TArray<FName> getSegmentationBoneNames(const UAnimSequenceBase& animation) const;
    TSharedPtr<FHandposeTimelinePlaybackController> playbackController;
    TWeakObjectPtr<UAnimSequenceBase> loadedAnimation;
    TWeakObjectPtr<UHandposeTimelineConfig> loadedConfig;
    TArray<FHandposeTimelineSegment> segments;
    int32 selectedSegmentIndex = INDEX_NONE;
    float timelineZoom = 1.0f;
    TSharedPtr<SHandposeTimelineTrack> timelineTrack;
    TWeakPtr<SWindow> handshapePickerWindow;
    TSharedPtr<STextBlock> configNameText;
    TSharedPtr<STextBlock> animationNameText;
    TSharedPtr<STextBlock> actorNameText;
    TSharedPtr<STextBlock> statusText;
    TSharedPtr<STextBlock> speedText;
    TSharedPtr<STextBlock> zoomText;
};
