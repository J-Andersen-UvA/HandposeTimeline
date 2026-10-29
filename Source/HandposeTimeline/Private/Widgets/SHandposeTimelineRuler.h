#pragma once

#include "Timeline/HandposeTimelineLayout.h"
#include "Widgets/SLeafWidget.h"

class UAnimSequenceBase;

DECLARE_DELEGATE_OneParam(FOnHandposeRulerScrubbed, float)

class SHandposeTimelineRuler : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeTimelineRuler)
        : _CurrentTimeSeconds(0.0f)
    {
    }
        SLATE_ATTRIBUTE(float, CurrentTimeSeconds)
        SLATE_EVENT(FOnHandposeRulerScrubbed, OnScrubbed)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setTimeline(
        UAnimSequenceBase* animation,
        const TArray<FHandposeTimelineSegment>& segments,
        float zoom);

    virtual FVector2D ComputeDesiredSize(float layoutScaleMultiplier) const override;
    virtual bool ComputeVolatility() const override
    {
        return currentTimeSeconds.IsBound() || SLeafWidget::ComputeVolatility();
    }
    virtual int32 OnPaint(
        const FPaintArgs& arguments,
        const FGeometry& allottedGeometry,
        const FSlateRect& clippingRect,
        FSlateWindowElementList& outDrawElements,
        int32 layerId,
        const FWidgetStyle& widgetStyle,
        bool parentEnabled) const override;
    virtual FReply OnMouseButtonDown(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    virtual FReply OnMouseMove(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;
    virtual FReply OnMouseButtonUp(
        const FGeometry& myGeometry,
        const FPointerEvent& mouseEvent) override;

private:
    void scrubToLocalX(float localX);
    int32 getTickInterval(float width) const;
    int32 getCurrentFrame() const;

    FHandposeTimelineLayout layout;
    TWeakObjectPtr<UAnimSequenceBase> animation;
    TAttribute<float> currentTimeSeconds;
    FOnHandposeRulerScrubbed onScrubbed;
};

class SHandposeTimelinePlayheadOverlay : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHandposeTimelinePlayheadOverlay)
        : _CurrentTimeSeconds(0.0f)
    {
    }
        SLATE_ATTRIBUTE(float, CurrentTimeSeconds)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setTimeline(
        UAnimSequenceBase* animation,
        const TArray<FHandposeTimelineSegment>& segments,
        float zoom);

    virtual FVector2D ComputeDesiredSize(float layoutScaleMultiplier) const override;
    virtual bool ComputeVolatility() const override
    {
        return currentTimeSeconds.IsBound() || SLeafWidget::ComputeVolatility();
    }
    virtual int32 OnPaint(
        const FPaintArgs& arguments,
        const FGeometry& allottedGeometry,
        const FSlateRect& clippingRect,
        FSlateWindowElementList& outDrawElements,
        int32 layerId,
        const FWidgetStyle& widgetStyle,
        bool parentEnabled) const override;

private:
    FHandposeTimelineLayout layout;
    TWeakObjectPtr<UAnimSequenceBase> animation;
    TAttribute<float> currentTimeSeconds;
};
