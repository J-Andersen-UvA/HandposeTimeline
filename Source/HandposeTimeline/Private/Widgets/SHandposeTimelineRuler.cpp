#include "Widgets/SHandposeTimelineRuler.h"

#include "Animation/AnimSequenceBase.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

namespace
{
constexpr float rulerHeight = 30.0f;
constexpr float minimumLabelSpacing = 72.0f;
}

void SHandposeTimelineRuler::Construct(const FArguments& arguments)
{
    currentTimeSeconds = arguments._CurrentTimeSeconds;
    onScrubbed = arguments._OnScrubbed;
}

void SHandposeTimelineRuler::setTimeline(
    UAnimSequenceBase* newAnimation,
    const TArray<FHandposeTimelineSegment>& segments,
    const float zoom)
{
    animation = newAnimation;
    layout.setTimeline(newAnimation, segments, zoom);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

FVector2D SHandposeTimelineRuler::ComputeDesiredSize(float) const
{
    return FVector2D(FMath::Max(1.0f, layout.getTotalWidth()), rulerHeight);
}

int32 SHandposeTimelineRuler::OnPaint(
    const FPaintArgs& arguments,
    const FGeometry& allottedGeometry,
    const FSlateRect& clippingRect,
    FSlateWindowElementList& outDrawElements,
    const int32 layerId,
    const FWidgetStyle& widgetStyle,
    const bool parentEnabled) const
{
    const ESlateDrawEffect drawEffects = parentEnabled
        ? ESlateDrawEffect::None
        : ESlateDrawEffect::DisabledEffect;
    const FSlateBrush* brush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
    const float width = allottedGeometry.GetLocalSize().X;

    FSlateDrawElement::MakeBox(
        outDrawElements,
        layerId,
        allottedGeometry.ToPaintGeometry(),
        brush,
        drawEffects,
        FLinearColor(0.045f, 0.045f, 0.045f, 1.0f));
    FSlateDrawElement::MakeBox(
        outDrawElements,
        layerId + 1,
        allottedGeometry.ToPaintGeometry(
            FVector2D(width, 1.0f),
            FSlateLayoutTransform(FVector2D(0.0f, rulerHeight - 1.0f))),
        brush,
        drawEffects,
        FLinearColor(0.35f, 0.35f, 0.35f, 1.0f));

    const int32 lastFrame = layout.getLastFrame();
    const int32 tickInterval = getTickInterval(width);
    for (int32 frame = 0; frame <= lastFrame; frame += tickInterval)
    {
        const float x = layout.getOffsetAtFrame(frame);
        FSlateDrawElement::MakeBox(
            outDrawElements,
            layerId + 1,
            allottedGeometry.ToPaintGeometry(
                FVector2D(1.0f, 7.0f),
                FSlateLayoutTransform(FVector2D(x, rulerHeight - 7.0f))),
            brush,
            drawEffects,
            FLinearColor(0.48f, 0.48f, 0.48f, 1.0f));
        FSlateDrawElement::MakeText(
            outDrawElements,
            layerId + 2,
            allottedGeometry.ToPaintGeometry(
                FVector2D(64.0f, 17.0f),
                FSlateLayoutTransform(FVector2D(x + 3.0f, 2.0f))),
            FText::AsNumber(frame),
            FAppStyle::GetFontStyle(TEXT("SmallFont")),
            drawEffects,
            FLinearColor(0.66f, 0.66f, 0.66f, 1.0f));
    }
    if (lastFrame > 0 && lastFrame % tickInterval != 0)
    {
        const float x = layout.getOffsetAtFrame(lastFrame);
        FSlateDrawElement::MakeText(
            outDrawElements,
            layerId + 2,
            allottedGeometry.ToPaintGeometry(
                FVector2D(60.0f, 17.0f),
                FSlateLayoutTransform(FVector2D(FMath::Max(0.0f, x - 30.0f), 2.0f))),
            FText::AsNumber(lastFrame),
            FAppStyle::GetFontStyle(TEXT("SmallFont")),
            drawEffects,
            FLinearColor(0.66f, 0.66f, 0.66f, 1.0f));
    }

    const int32 currentFrame = getCurrentFrame();
    const float playheadX = layout.getOffsetAtFrame(currentFrame);
    const float labelX = FMath::Clamp(playheadX - 28.0f, 0.0f, FMath::Max(0.0f, width - 72.0f));
    FSlateDrawElement::MakeBox(
        outDrawElements,
        layerId + 3,
        allottedGeometry.ToPaintGeometry(
            FVector2D(72.0f, 17.0f),
            FSlateLayoutTransform(FVector2D(labelX, 0.0f))),
        brush,
        drawEffects,
        FLinearColor(0.12f, 0.12f, 0.12f, 0.96f));
    FSlateDrawElement::MakeText(
        outDrawElements,
        layerId + 4,
        allottedGeometry.ToPaintGeometry(
            FVector2D(68.0f, 17.0f),
            FSlateLayoutTransform(FVector2D(labelX + 4.0f, 1.0f))),
        FText::Format(
            NSLOCTEXT("HandposeTimelineRuler", "CurrentFrame", "Frame {0}"),
            FText::AsNumber(currentFrame)),
        FAppStyle::GetFontStyle(TEXT("SmallFont")),
        drawEffects,
        FLinearColor::White);
    FSlateDrawElement::MakeBox(
        outDrawElements,
        layerId + 4,
        allottedGeometry.ToPaintGeometry(
            FVector2D(7.0f, 7.0f),
            FSlateLayoutTransform(FVector2D(playheadX - 3.0f, rulerHeight - 7.0f))),
        brush,
        drawEffects,
        FLinearColor(0.88f, 0.88f, 0.88f, 1.0f));
    return layerId + 5;
}

FReply SHandposeTimelineRuler::OnMouseButtonDown(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (mouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }
    scrubToLocalX(myGeometry.AbsoluteToLocal(mouseEvent.GetScreenSpacePosition()).X);
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SHandposeTimelineRuler::OnMouseMove(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (!HasMouseCapture())
    {
        return FReply::Unhandled();
    }
    scrubToLocalX(myGeometry.AbsoluteToLocal(mouseEvent.GetScreenSpacePosition()).X);
    return FReply::Handled();
}

FReply SHandposeTimelineRuler::OnMouseButtonUp(
    const FGeometry& myGeometry,
    const FPointerEvent& mouseEvent)
{
    if (mouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
    {
        return FReply::Unhandled();
    }
    scrubToLocalX(myGeometry.AbsoluteToLocal(mouseEvent.GetScreenSpacePosition()).X);
    return FReply::Handled().ReleaseMouseCapture();
}

void SHandposeTimelineRuler::scrubToLocalX(const float localX)
{
    const float totalWidth = layout.getTotalWidth();
    if (totalWidth > 0.0f)
    {
        onScrubbed.ExecuteIfBound(layout.getTimeAtSliderValue(
            FMath::Clamp(localX / totalWidth, 0.0f, 1.0f)));
    }
}

int32 SHandposeTimelineRuler::getTickInterval(const float width) const
{
    const int32 lastFrame = FMath::Max(1, layout.getLastFrame());
    const float targetTickCount = FMath::Max(1.0f, width / minimumLabelSpacing);
    const float rawInterval = lastFrame / targetTickCount;
    const float magnitude = FMath::Pow(10.0f, FMath::FloorToFloat(FMath::LogX(10.0f, FMath::Max(1.0f, rawInterval))));
    const float normalized = rawInterval / magnitude;
    const float niceInterval = normalized <= 1.0f
        ? 1.0f
        : normalized <= 2.0f
            ? 2.0f
            : normalized <= 5.0f
                ? 5.0f
                : 10.0f;
    return FMath::Max(1, FMath::RoundToInt(niceInterval * magnitude));
}

int32 SHandposeTimelineRuler::getCurrentFrame() const
{
    const UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        return 0;
    }
    return FMath::Clamp(
        FMath::RoundToInt(
            currentTimeSeconds.Get(0.0f) *
            loadedAnimation->GetSamplingFrameRate().AsDecimal()),
        0,
        layout.getLastFrame());
}

void SHandposeTimelinePlayheadOverlay::Construct(const FArguments& arguments)
{
    currentTimeSeconds = arguments._CurrentTimeSeconds;
    SetVisibility(EVisibility::HitTestInvisible);
}

void SHandposeTimelinePlayheadOverlay::setTimeline(
    UAnimSequenceBase* newAnimation,
    const TArray<FHandposeTimelineSegment>& segments,
    const float zoom)
{
    animation = newAnimation;
    layout.setTimeline(newAnimation, segments, zoom);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

FVector2D SHandposeTimelinePlayheadOverlay::ComputeDesiredSize(float) const
{
    return FVector2D(FMath::Max(1.0f, layout.getTotalWidth()), 1.0f);
}

int32 SHandposeTimelinePlayheadOverlay::OnPaint(
    const FPaintArgs& arguments,
    const FGeometry& allottedGeometry,
    const FSlateRect& clippingRect,
    FSlateWindowElementList& outDrawElements,
    const int32 layerId,
    const FWidgetStyle& widgetStyle,
    const bool parentEnabled) const
{
    const UAnimSequenceBase* loadedAnimation = animation.Get();
    if (loadedAnimation == nullptr || !loadedAnimation->GetSamplingFrameRate().IsValid())
    {
        return layerId;
    }
    const int32 currentFrame = FMath::Clamp(
        FMath::RoundToInt(
            currentTimeSeconds.Get(0.0f) *
            loadedAnimation->GetSamplingFrameRate().AsDecimal()),
        0,
        layout.getLastFrame());
    const float x = layout.getOffsetAtFrame(currentFrame);
    TArray<FVector2D> linePoints = {
        FVector2D(x, rulerHeight),
        FVector2D(x, allottedGeometry.GetLocalSize().Y)
    };
    FSlateDrawElement::MakeLines(
        outDrawElements,
        layerId,
        allottedGeometry.ToPaintGeometry(),
        linePoints,
        parentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
        FLinearColor(0.88f, 0.88f, 0.88f, 0.82f),
        true,
        1.0f);
    return layerId + 1;
}
