#pragma once

#include "CoreMinimal.h"

class SWidget;
class UAnimSequence;
class UAnimSequenceBase;

struct FHandposeTimelineExtensionContext
{
    TWeakObjectPtr<UAnimSequenceBase> animation;
    FFrameRate frameRate;
    int32 lastFrame = 0;
    int32 trimStartFrame = 0;
    int32 trimEndFrame = 0;
    int32 currentFrame = 0;
    float pixelsPerFrame = 1.0f;
    TFunction<void(const FText&)> setStatus;
    TFunction<void(int32)> scrubToFrame;
    TFunction<void(bool)> requestPreviewRebuild;

    float getTotalWidth() const
    {
        return FMath::Max(1.0f, static_cast<float>(lastFrame + 1) * pixelsPerFrame);
    }

    float getOffsetAtFrame(const double frame) const
    {
        return FMath::Clamp(
            static_cast<float>(frame) * pixelsPerFrame,
            0.0f,
            getTotalWidth());
    }

    int32 getFrameAtOffset(const float offset) const
    {
        return FMath::Clamp(
            FMath::RoundToInt(offset / FMath::Max(pixelsPerFrame, UE_SMALL_NUMBER)),
            0,
            lastFrame);
    }
};

class IHandposeTimelineExtension
{
public:
    virtual ~IHandposeTimelineExtension() = default;
    virtual FName getExtensionId() const = 0;
    virtual FText getDisplayName() const = 0;
    virtual FText getTooltip() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual void onToolbarClicked()
    {
        setEnabled(!isEnabled());
    }
    virtual TSharedRef<SWidget> createLane() = 0;
    virtual void updateContext(const FHandposeTimelineExtensionContext& context) = 0;
    virtual bool hasPreviewChanges() const
    {
        return false;
    }
    virtual UAnimSequence* buildPreview(UAnimSequence& source, FString& errorMessage)
    {
        return &source;
    }
    virtual bool deleteSelection()
    {
        return false;
    }
};
