#include "Widgets/SHandposeTimelineWidget.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/DataTable.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FWordingTiming
{
    double start = 0;
    double duration = 0;
    double leadIn = 0;
    double leadOut = 0;

    double end() const { return start + duration; }
    double blendStart() const { return start - leadIn; }
    double blendEnd() const { return end() + leadOut; }

    static FWordingTiming fromBlock(const FWordingBlock& block)
    {
        return {block.start, block.duration, block.leadIn, block.leadOut};
    }

    void applyTo(FWordingBlock& block) const
    {
        block.start = start;
        block.duration = duration;
        block.leadIn = leadIn;
        block.leadOut = leadOut;
    }
};

class SWordingLane : public SLeafWidget
{
public:
    using FOpen = TFunction<void(double, int32)>;
    using FChange = TFunction<void(int32, const FWordingTiming&)>;
    using FDelete = TFunction<void(int32)>;

    SLATE_BEGIN_ARGS(SWordingLane) {}
        SLATE_ARGUMENT(const FHandposeTimelineLayout*, Layout)
        SLATE_ARGUMENT(const TArray<FWordingBlock>*, Blocks)
        SLATE_ATTRIBUTE(double, Fps)
        SLATE_ATTRIBUTE(double, Length)
        SLATE_ATTRIBUTE(bool, CanEdit)
        SLATE_ARGUMENT(FOpen, OnOpen)
        SLATE_ARGUMENT(FOpen, OnMarkViseme)
        SLATE_ARGUMENT(FChange, OnChange)
        SLATE_ARGUMENT(FDelete, OnDelete)
    SLATE_END_ARGS()

    void Construct(const FArguments& args)
    {
        layout = args._Layout;
        blocks = args._Blocks;
        fps = args._Fps;
        length = args._Length;
        canEdit = args._CanEdit;
        onOpen = args._OnOpen;
        onMarkViseme = args._OnMarkViseme;
        onChange = args._OnChange;
        onDelete = args._OnDelete;
    }

    FVector2D ComputeDesiredSize(float) const override
    {
        return FVector2D(layout->getTotalWidth(), 42);
    }

    int32 OnPaint(const FPaintArgs&, const FGeometry& geometry, const FSlateRect&,
        FSlateWindowElementList& elements, int32 layer, const FWidgetStyle&, bool) const override
    {
        const auto* brush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
        FSlateDrawElement::MakeBox(elements, layer, geometry.ToPaintGeometry(),
            brush, ESlateDrawEffect::None, FLinearColor(0.07, 0.07, 0.07));
        if (fps.Get(0) <= 0)
        {
            return layer;
        }

        const auto box = [&](int32 depth, float x, float y, float width, float height, FLinearColor color)
        {
            if (width > 0)
            {
                FSlateDrawElement::MakeBox(elements, layer + depth,
                    geometry.ToPaintGeometry(FVector2f(width, height), FSlateLayoutTransform(FVector2f(x, y))),
                    brush, ESlateDrawEffect::None, color);
            }
        };
        for (int32 index = 0; index < blocks->Num(); ++index)
        {
            const auto& block = (*blocks)[index];
            const FWordingTiming timing = index == drag.index ? draft : FWordingTiming::fromBlock(block);
            const float outerLeft = offset(timing.blendStart());
            const float left = offset(timing.start);
            const float right = offset(timing.end());
            const float outerRight = offset(timing.blendEnd());

            box(1, outerLeft, 3, left - outerLeft, 36, FLinearColor(0.14, 0.25, 0.35));
            box(1, left, 3, right - left, 36, index == drag.index
                ? FLinearColor(0.18, 0.53, 0.68) : FLinearColor(0.12, 0.42, 0.57));
            box(1, right, 3, outerRight - right, 36, FLinearColor(0.14, 0.25, 0.35));

            // The upper handles resize speech. The lower edges resize the blends.
            const auto drawHandle = [&](EHandle handle, float x, float y, float height, float width)
            {
                const bool active = (hover.index == index && hover.handle == handle) ||
                    (drag.index == index && drag.handle == handle);
                box(3, FMath::Clamp(x - width * 0.5f, 0.0f,
                    FMath::Max(0.0f, layout->getTotalWidth() - width)), y, width, height,
                    active ? FLinearColor::White : FLinearColor(0.65, 0.80, 0.88));
            };
            drawHandle(EHandle::SpeechStart, left, 3, 10, 7);
            drawHandle(EHandle::SpeechEnd, right, 3, 10, 7);
            drawHandle(EHandle::BlendStart, outerLeft, 14, 25, 2);
            drawHandle(EHandle::BlendEnd, outerRight, 14, 25, 2);

            for (const auto& marker : block.markers)
            {
                const float markerX = offset(timing.start + marker.position * timing.duration);
                box(3, markerX - 1, 13, 2, 23, FLinearColor(1.0, 0.74, 0.27));
            }
            if (right > left + 8)
            {
                const auto label = geometry.ToPaintGeometry(
                    FVector2f(right - left - 8, 21), FSlateLayoutTransform(FVector2f(left + 4, 16)));
                elements.PushClip(FSlateClippingZone(label));
                FSlateDrawElement::MakeText(elements, layer + 2, label, block.text,
                    FAppStyle::GetFontStyle(TEXT("SmallFont")), ESlateDrawEffect::None, FLinearColor::White);
                elements.PopClip();
            }
        }
        return layer + 3;
    }

    FReply OnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override
    {
        const FKey button = event.GetEffectingButton();
        if ((button != EKeys::LeftMouseButton && button != EKeys::RightMouseButton) ||
            fps.Get(0) <= 0 || !canEdit.Get(false))
        {
            return FReply::Unhandled();
        }
        const FVector2D local = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
        const FHit hit = hitTest(local);
        const double time = frameTime(local.X);
        if (button == EKeys::RightMouseButton)
        {
            if (!blocks->IsValidIndex(hit.index))
            {
                return FReply::Unhandled();
            }
            const TWeakPtr<SWordingLane> weak = SharedThis(this);
            FMenuBuilder menu(true, nullptr);
            menu.AddMenuEntry(FText::FromString(TEXT("Edit Wording…")),
                FText::FromString(TEXT("Edit the text, speech timing, and blends.")), FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([weak, time, hit]
                {
                    if (const auto lane = weak.Pin(); lane && lane->canEdit.Get(false))
                    {
                        lane->onOpen(time, hit.index);
                    }
                })));
            menu.AddMenuEntry(FText::FromString(TEXT("Mark Viseme")),
                FText::FromString(TEXT("Adjust how strongly the generated curves move around this frame.")), FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([weak, time, hit]
                {
                    if (const auto lane = weak.Pin(); lane && lane->canEdit.Get(false))
                    {
                        lane->onMarkViseme(time, hit.index);
                    }
                })));
            menu.AddMenuEntry(FText::FromString(TEXT("Delete Wording")), FText::GetEmpty(), FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([weak, hit]
                {
                    if (const auto lane = weak.Pin(); lane && lane->canEdit.Get(false))
                    {
                        lane->onDelete(hit.index);
                    }
                })));
            FSlateApplication::Get().PushMenu(SharedThis(this), event.GetEventPath() ? *event.GetEventPath() : FWidgetPath{},
                menu.MakeWidget(), event.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);
            return FReply::Handled();
        }
        if (!blocks->IsValidIndex(hit.index))
        {
            onOpen(time, INDEX_NONE);
            return FReply::Handled();
        }
        drag = hit;
        original = FWordingTiming::fromBlock((*blocks)[hit.index]);
        draft = original;
        dragTime = time;
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    FReply OnMouseMove(const FGeometry& geometry, const FPointerEvent& event) override
    {
        const FVector2D local = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
        if (HasMouseCapture() && blocks->IsValidIndex(drag.index))
        {
            updateDraft(frameTime(local.X));
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }
        const FHit next = hitTest(local);
        if (hover.index != next.index || hover.handle != next.handle)
        {
            hover = next;
            Invalidate(EInvalidateWidgetReason::Paint);
        }
        return FReply::Unhandled();
    }

    FReply OnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event) override
    {
        if (event.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
        {
            return FReply::Unhandled();
        }
        if (blocks->IsValidIndex(drag.index) && canEdit.Get(false))
        {
            updateDraft(frameTime(geometry.AbsoluteToLocal(event.GetScreenSpacePosition()).X));
            const int32 index = drag.index;
            drag = {};
            if (draft.start != original.start || draft.duration != original.duration ||
                draft.leadIn != original.leadIn || draft.leadOut != original.leadOut)
            {
                onChange(index, draft);
            }
        }
        drag = {};
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled().ReleaseMouseCapture();
    }

    void OnMouseCaptureLost(const FCaptureLostEvent& event) override
    {
        drag = {};
        Invalidate(EInvalidateWidgetReason::Paint);
        SLeafWidget::OnMouseCaptureLost(event);
    }

    void OnMouseLeave(const FPointerEvent& event) override
    {
        hover = {};
        Invalidate(EInvalidateWidgetReason::Paint);
        SLeafWidget::OnMouseLeave(event);
    }

    FCursorReply OnCursorQuery(const FGeometry& geometry, const FPointerEvent& event) const override
    {
        if (!canEdit.Get(false))
        {
            return FCursorReply::Unhandled();
        }
        const FHit hit = HasMouseCapture() ? drag : hitTest(geometry.AbsoluteToLocal(event.GetScreenSpacePosition()));
        if (hit.index == INDEX_NONE)
        {
            return FCursorReply::Unhandled();
        }
        return FCursorReply::Cursor(hit.handle == EHandle::Move ? EMouseCursor::CardinalCross : EMouseCursor::ResizeLeftRight);
    }

private:
    enum class EHandle { Move, SpeechStart, SpeechEnd, BlendStart, BlendEnd };
    struct FHit
    {
        int32 index = INDEX_NONE;
        EHandle handle = EHandle::Move;
    };

    float offset(double seconds) const
    {
        return layout->getOffsetAtFrame(FMath::Clamp(seconds, 0.0, length.Get(0)) * fps.Get(0));
    }

    double frameTime(float x) const
    {
        const double rate = fps.Get(0);
        return rate > 0 ? FMath::RoundToDouble(layout->getFrameAtOffset(x)) / rate : 0.0;
    }

    FHit hitTest(FVector2D local) const
    {
        FHit edge;
        FHit body;
        float best = 5.0f;
        for (int32 index = 0; index < blocks->Num(); ++index)
        {
            const FWordingTiming timing = FWordingTiming::fromBlock((*blocks)[index]);
            const float left = offset(timing.blendStart());
            const float right = offset(timing.blendEnd());
            const auto checkEdge = [&](EHandle handle, float x)
            {
                const float distance = FMath::Abs(static_cast<float>(local.X) - x);
                if (distance <= best)
                {
                    best = distance;
                    edge = {index, handle};
                }
            };
            if (local.Y < 14)
            {
                checkEdge(EHandle::SpeechStart, offset(timing.start));
                checkEdge(EHandle::SpeechEnd, offset(timing.end()));
            }
            else
            {
                checkEdge(EHandle::BlendStart, left);
                checkEdge(EHandle::BlendEnd, right);
            }
            if (local.X >= left && local.X <= right)
            {
                body = {index, EHandle::Move};
            }
        }
        return edge.index != INDEX_NONE ? edge : body;
    }

    void updateDraft(double time)
    {
        draft = original;
        const double delta = time - dragTime;
        if (FMath::IsNearlyZero(delta))
        {
            return;
        }
        const double frame = 1.0 / fps.Get();
        const double minDuration = FMath::Min(original.duration, frame);
        // Allow the visible portions of existing blends that reach past the animation boundary.
        double left = FMath::Min(0.0, original.blendStart());
        double right = FMath::Max(length.Get(), original.blendEnd());
        for (int32 index = 0; index < blocks->Num(); ++index)
        {
            if (index == drag.index)
            {
                continue;
            }
            const FWordingTiming item = FWordingTiming::fromBlock((*blocks)[index]);
            if (item.blendEnd() < original.blendStart())
            {
                left = FMath::Max(left, item.blendEnd() + UE_KINDA_SMALL_NUMBER);
            }
            else if (item.blendStart() > original.blendEnd())
            {
                right = FMath::Min(right, item.blendStart() - UE_KINDA_SMALL_NUMBER);
            }
        }

        // Movement and speech resizing retain the blend lengths when there is room.
        const double startFloor = FMath::Min(original.start, FMath::Max(0.0, left + original.leadIn));
        const double endCeiling = FMath::Max(original.end(), FMath::Min(length.Get(), right - original.leadOut));
        switch (drag.handle)
        {
        case EHandle::Move:
            draft.start = original.start + FMath::Clamp(delta, startFloor - original.start, endCeiling - original.end());
            break;
        case EHandle::SpeechStart:
            draft.start = FMath::Clamp(original.start + delta, startFloor, original.end() - minDuration);
            draft.duration = original.end() - draft.start;
            break;
        case EHandle::SpeechEnd:
            draft.duration = FMath::Clamp(original.end() + delta, original.start + minDuration, endCeiling) - original.start;
            break;
        case EHandle::BlendStart:
            draft.leadIn = original.start - FMath::Clamp(FMath::Max(0.0, original.blendStart()) + delta,
                FMath::Max(0.0, left), original.start);
            break;
        case EHandle::BlendEnd:
            draft.leadOut = FMath::Clamp(FMath::Min(length.Get(), original.blendEnd()) + delta,
                original.end(), FMath::Min(length.Get(), right)) - original.end();
            break;
        }
    }

    const FHandposeTimelineLayout* layout = nullptr;
    const TArray<FWordingBlock>* blocks = nullptr;
    TAttribute<double> fps;
    TAttribute<double> length;
    TAttribute<bool> canEdit;
    FOpen onOpen;
    FOpen onMarkViseme;
    FChange onChange;
    FDelete onDelete;
    FHit hover;
    FHit drag;
    FWordingTiming original;
    FWordingTiming draft;
    double dragTime = 0;
};

TSharedRef<SWindow> wordingWindow(const TCHAR* title, FVector2D size)
{
    return SNew(SWindow).Title(FText::FromString(title)).ClientSize(size)
        .SupportsMaximize(false).SupportsMinimize(false);
}
}

TSharedRef<SWidget> SHandposeTimelineWidget::makeWordingLane()
{
    return SNew(SBox)
        .Visibility_Lambda([this] { return Wording::available() && wordingEnabled ? EVisibility::Visible : EVisibility::Collapsed; })
        .Padding(FMargin(12, 6))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Text(FText::FromString(TEXT("Wording")))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
            [
                SAssignNew(wordingLane, SWordingLane)
                .Layout(&wordingLayout)
                .Blocks(&wordingBlocks)
                .Fps_Lambda([this] { return loadedAnimation.IsValid() ? loadedAnimation->GetSamplingFrameRate().AsDecimal() : 0.0; })
                .Length_Lambda([this] { return loadedAnimation.IsValid() ? loadedAnimation->GetPlayLength() : 0.0; })
                .CanEdit_Lambda([this] { return wordingEnabled && !wordingJob.IsValid() && !wordingBlockWindow.IsValid(); })
                .OnOpen([this](double time, int32 index) { openWordingBlock(time, index); })
                .OnMarkViseme([this](double time, int32 index) { openVisemeMarker(time, index); })
                .OnChange([this](int32 index, const FWordingTiming& timing)
                {
                    if (wordingBlocks.IsValidIndex(index))
                    {
                        FWordingBlock updated = wordingBlocks[index];
                        timing.applyTo(updated);
                        FString error;
                        if (!commitWording(MoveTemp(updated), index, error))
                        {
                            statusText->SetText(FText::FromString(error));
                        }
                    }
                })
                .OnDelete([this](int32 index) { deleteWordingBlock(index); })
            ]
        ];
}

void SHandposeTimelineWidget::refreshWording()
{
    wordingLayout.setTimeline(loadedAnimation.Get(), rightSegments, timelineZoom);
    if (wordingLane)
    {
        wordingLane->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
}

void SHandposeTimelineWidget::resetWording()
{
    stopWordingAudio();
    for (const auto& weakWindow : {wordingSettingsWindow, wordingBlockWindow})
    {
        if (const auto window = weakWindow.Pin())
        {
            window->RequestDestroyWindow();
        }
    }
    wordingJob.Reset();
    wordingBlocks.Reset();
    refreshWording();
}

FReply SHandposeTimelineWidget::onCorrectWordingClicked()
{
    if (!Wording::available())
    {
        return FReply::Handled();
    }
    if (const auto existing = wordingSettingsWindow.Pin())
    {
        existing->BringToFront(true);
        return FReply::Handled();
    }

    if (!wordingFaceMap.IsValid())
    {
        wordingFaceMap.Reset(LoadObject<UDataTable>(nullptr,
            TEXT("/Game/MappingsCSV/CCARKitMapping.CCARKitMapping")));
    }
    if (!wordingTongueMap.IsValid())
    {
        wordingTongueMap.Reset(LoadObject<UDataTable>(nullptr,
            TEXT("/Game/MappingsCSV/CCA2FTongueMapping.CCA2FTongueMapping")));
    }

    const auto window = wordingWindow(TEXT("Correct Wording"), FVector2D(530, 520));
    wordingSettingsWindow = window;
    const auto weak = SharedThis(this).ToWeakPtr();
    const auto mappings = SNew(SVerticalBox);
    const auto channels = SNew(SVerticalBox);
    const auto populateChannels = [weak, channels]
    {
        const auto self = weak.Pin();
        if (!self)
        {
            return;
        }
        TSet<FName> names;
        for (const auto* table : {self->wordingFaceMap.Get(), self->wordingTongueMap.Get()})
        {
            if (table)
            {
                for (FName name : table->GetRowNames()) { names.Add(name); }
            }
        }
        TArray<FName> sorted = names.Array();
        sorted.Sort(FNameLexicalLess());
        channels->ClearChildren();
        for (FName name : sorted)
        {
            channels->AddSlot().AutoHeight()
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([weak, name]
                {
                    const auto owner = weak.Pin();
                    const bool* enabled = owner ? owner->wordingChannelOverrides.Find(name) : nullptr;
                    return (enabled ? *enabled : Wording::defaultChannel(name)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([weak, name](ECheckBoxState state)
                {
                    if (const auto owner = weak.Pin())
                    {
                        owner->wordingChannelOverrides.Add(name, state == ECheckBoxState::Checked);
                        owner->rebuildPreviewAnimation(true);
                    }
                })
                [SNew(STextBlock).Text(FText::FromName(name))]
            ];
        }
    };
    populateChannels();

    for (bool tongue : {false, true})
    {
        mappings->AddSlot().AutoHeight().Padding(0, 4)
        [
            SNew(STextBlock).Text(FText::FromString(tongue ? TEXT("Tongue mapping") : TEXT("Face mapping")))
        ];
        mappings->AddSlot().AutoHeight()
        [
            SNew(SObjectPropertyEntryBox)
            .AllowedClass(UDataTable::StaticClass())
            .ObjectPath_Lambda([weak, tongue]
            {
                if (const auto self = weak.Pin())
                {
                    auto* table = tongue ? self->wordingTongueMap.Get() : self->wordingFaceMap.Get();
                    if (table) { return table->GetPathName(); }
                }
                return FString{};
            })
            .OnObjectChanged_Lambda([weak, tongue, populateChannels](const FAssetData& asset)
            {
                if (const auto self = weak.Pin())
                {
                    auto& table = tongue ? self->wordingTongueMap : self->wordingFaceMap;
                    table.Reset(Cast<UDataTable>(asset.GetAsset()));
                    populateChannels();
                    self->rebuildPreviewAnimation(true);
                }
            })
        ];
    }

    window->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(12, 8)
        [
            SNew(SCheckBox)
            .IsChecked_Lambda([weak]
            {
                const auto self = weak.Pin();
                return self && self->wordingEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([weak](ECheckBoxState state)
            {
                if (const auto self = weak.Pin())
                {
                    self->wordingEnabled = state == ECheckBoxState::Checked;
                    self->refreshWording();
                    self->rebuildPreviewAnimation(true);
                }
            })
            [SNew(STextBlock).Text(FText::FromString(TEXT("Enable wording lane")))]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(12, 2)
        [SNew(STextBlock).Text(FText::FromString(TEXT("Piper voice profile")))]
        + SVerticalBox::Slot().AutoHeight().Padding(12, 2)
        [
            SNew(SObjectPropertyEntryBox)
            .AllowedClass(Wording::voiceClass())
            .ObjectPath_Lambda([weak]
            {
                const auto self = weak.Pin();
                return self && self->wordingVoice.IsValid() ? self->wordingVoice->GetPathName() : FString{};
            })
            .OnObjectChanged_Lambda([weak](const FAssetData& asset)
            {
                if (const auto self = weak.Pin()) { self->wordingVoice.Reset(asset.GetAsset()); }
            })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(12, 2)[mappings]
        + SVerticalBox::Slot().AutoHeight().Padding(12, 8)
        [
            SNew(SCheckBox)
            .ToolTipText(FText::FromString(TEXT("Apply every wording curve with an avatar mapping during the wording block.")))
            .IsChecked_Lambda([weak]
            {
                const auto self = weak.Pin();
                return self && self->wordingOverwriteAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([weak](ECheckBoxState state)
            {
                if (const auto self = weak.Pin())
                {
                    self->wordingOverwriteAll = state == ECheckBoxState::Checked;
                    self->rebuildPreviewAnimation(true);
                }
            })
            [SNew(STextBlock).Text(FText::FromString(TEXT("Overwrite All")))]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(12, 2)
        [SNew(STextBlock).Text(FText::FromString(TEXT("Enabled source curves")))]
        + SVerticalBox::Slot().FillHeight(1).Padding(12, 0, 12, 12)
        [
            SNew(SScrollBox)
            .IsEnabled_Lambda([weak]
            {
                const auto self = weak.Pin();
                return self && !self->wordingOverwriteAll;
            })
            + SScrollBox::Slot()[channels]
        ]);
    FSlateApplication::Get().AddWindow(window);
    return FReply::Handled();
}

void SHandposeTimelineWidget::openWordingBlock(double time, int32 index)
{
    if (wordingJob.IsValid() || !Cast<UAnimSequence>(loadedAnimation.Get()))
    {
        return;
    }
    if (const auto existing = wordingBlockWindow.Pin())
    {
        existing->BringToFront(true);
        return;
    }

    const bool editing = wordingBlocks.IsValidIndex(index);
    const FWordingBlock* original = editing ? &wordingBlocks[index] : nullptr;
    const auto window = wordingWindow(editing ? TEXT("Edit Wording") : TEXT("Insert Wording"), FVector2D(470, 320));
    wordingBlockWindow = window;
    const auto weak = SharedThis(this).ToWeakPtr();
    const TWeakPtr<SWindow> weakWindow = window;
    const auto phrase = SNew(SEditableTextBox)
        .Text(FText::FromString(editing ? original->text : FString{}))
        .HintText(FText::FromString(TEXT("Word or sentence")));
    const auto start = SNew(SEditableTextBox).Text(FText::AsNumber(editing ? original->start : time));
    const auto duration = SNew(SEditableTextBox)
        .Text(FText::FromString(editing ? FString::SanitizeFloat(original->duration) : FString{}))
        .HintText(FText::FromString(TEXT("Use generated audio length")));
    start->SetText(FText::FromString(FString::SanitizeFloat(editing ? original->start : time)));
    const auto in = SNew(SEditableTextBox).Text(FText::FromString(FString::SanitizeFloat(editing ? original->leadIn : 0.08)));
    const auto out = SNew(SEditableTextBox).Text(FText::FromString(FString::SanitizeFloat(editing ? original->leadOut : 0.08)));
    const auto message = SNew(STextBlock).AutoWrapText(true);
    const auto body = SNew(SVerticalBox);
    body->AddSlot().AutoHeight().Padding(10, 8)[phrase];
    const TCHAR* labels[] = {TEXT("Speech start (seconds)"), TEXT("Speech duration (seconds)"),
        TEXT("Lead-in blend (seconds)"), TEXT("Lead-out blend (seconds)")};
    const TSharedRef<SEditableTextBox> fields[] = {start, duration, in, out};
    for (int32 field = 0; field < UE_ARRAY_COUNT(labels); ++field)
    {
        body->AddSlot().AutoHeight().Padding(10, 3)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(labels[field]))]
            + SHorizontalBox::Slot().FillWidth(1)[fields[field]]
        ];
    }
    body->AddSlot().AutoHeight().Padding(10, 5)[message];
    body->AddSlot().AutoHeight().Padding(10, 6)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton).Text(FText::FromString(editing ? TEXT("Apply") : TEXT("Generate")))
            .OnClicked_Lambda([weak, weakWindow, phrase, start, duration, in, out, message, index]
            {
                const auto self = weak.Pin();
                if (!self) { return FReply::Handled(); }
                const auto fail = [&message](const FString& text)
                {
                    message->SetText(FText::FromString(text));
                    return FReply::Handled();
                };
                FWordingBlock block;
                block.text = phrase->GetText().ToString().TrimStartAndEnd();
                const FString durationText = duration->GetText().ToString().TrimStartAndEnd();
                if (block.text.IsEmpty() ||
                    !LexTryParseString(block.start, *start->GetText().ToString()) ||
                    (!durationText.IsEmpty() && !LexTryParseString(block.duration, *durationText)) ||
                    !LexTryParseString(block.leadIn, *in->GetText().ToString()) ||
                    !LexTryParseString(block.leadOut, *out->GetText().ToString()) ||
                    !FMath::IsFinite(block.start) || !FMath::IsFinite(block.duration) ||
                    !FMath::IsFinite(block.leadIn) || !FMath::IsFinite(block.leadOut) ||
                    block.start < 0 || block.duration < 0 || block.leadIn < 0 || block.leadOut < 0)
                {
                    return fail(TEXT("Enter a phrase and valid non-negative times."));
                }
                FString error;
                if (self->wordingBlocks.IsValidIndex(index) && self->wordingBlocks[index].text == block.text)
                {
                    block.clip = self->wordingBlocks[index].clip;
                    block.audio = self->wordingBlocks[index].audio;
                    block.markers = self->wordingBlocks[index].markers;
                    if (!self->commitWording(MoveTemp(block), index, error))
                    {
                        return fail(error);
                    }
                }
                else
                {
                    if (!self->wordingVoice.IsValid())
                    {
                        return fail(TEXT("Choose a Piper voice profile in Correct Wording settings."));
                    }
                    UObject* job = Wording::generate(block.text, self->wordingVoice.Get(), block.audio, error);
                    if (!job) { return fail(error); }
                    self->wordingJob.Reset(job);
                    self->pendingWording = MoveTemp(block);
                    self->pendingWordingIndex = index;
                    self->statusText->SetText(FText::FromString(TEXT("Generating wording…")));
                    self->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(
                        self.ToSharedRef(), &SHandposeTimelineWidget::pollWording));
                }
                if (const auto dialog = weakWindow.Pin()) { dialog->RequestDestroyWindow(); }
                return FReply::Handled();
            })
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SButton).Text(FText::FromString(TEXT("Delete")))
            .Visibility(editing ? EVisibility::Visible : EVisibility::Collapsed)
            .OnClicked_Lambda([weak, weakWindow, index]
            {
                if (const auto self = weak.Pin())
                {
                    self->deleteWordingBlock(index);
                }
                if (const auto dialog = weakWindow.Pin()) { dialog->RequestDestroyWindow(); }
                return FReply::Handled();
            })
        ]
    ];
    window->SetContent(body);
    window->SetWidgetToFocusOnActivate(phrase);
    FSlateApplication::Get().AddWindow(window);
}

void SHandposeTimelineWidget::deleteWordingBlock(int32 index)
{
    if (wordingBlocks.IsValidIndex(index))
    {
        wordingBlocks.RemoveAt(index);
        refreshWording();
        rebuildPreviewAnimation(true);
    }
}

bool SHandposeTimelineWidget::commitWording(FWordingBlock block, int32 index, FString& error)
{
    if (!loadedAnimation.IsValid())
    {
        error = TEXT("Load an animation before adding wording.");
        return false;
    }
    if (block.duration == 0) { block.duration = block.clip.duration; }
    const double begin = FMath::Max(0.0, block.start - block.leadIn);
    const double end = block.start + block.duration + block.leadOut;
    if (block.start + block.duration > loadedAnimation->GetPlayLength() + UE_SMALL_NUMBER)
    {
        error = TEXT("The spoken region extends past the animation. Start earlier or shorten its duration.");
        return false;
    }
    for (int32 other = 0; other < wordingBlocks.Num(); ++other)
    {
        const auto& item = wordingBlocks[other];
        if (other != index && begin <= item.start + item.duration + item.leadOut && end >= item.start - item.leadIn)
        {
            error = TEXT("Wording blocks and their blends need separate time ranges.");
            return false;
        }
    }
    if (wordingBlocks.IsValidIndex(index)) { wordingBlocks[index] = MoveTemp(block); }
    else { wordingBlocks.Add(MoveTemp(block)); }
    refreshWording();
    rebuildPreviewAnimation(true);
    return true;
}

EActiveTimerReturnType SHandposeTimelineWidget::pollWording(double, float)
{
    if (!wordingJob.IsValid()) { return EActiveTimerReturnType::Stop; }
    FString error;
    if (!Wording::readResult(*wordingJob.Get(), pendingWording.clip, error))
    {
        return EActiveTimerReturnType::Continue;
    }
    wordingJob.Reset();
    if (error.IsEmpty())
    {
        statusText->SetText(FText::FromString(TEXT("Wording generated.")));
        commitWording(MoveTemp(pendingWording), pendingWordingIndex, error);
    }
    if (!error.IsEmpty())
    {
        statusText->SetText(FText::FromString(error));
    }
    pendingWording = {};
    return EActiveTimerReturnType::Stop;
}

bool SHandposeTimelineWidget::hasWording() const
{
    return wordingEnabled && !wordingBlocks.IsEmpty();
}



void SHandposeTimelineWidget::openVisemeMarker(double time, int32 blockIndex)
{
    if (!wordingBlocks.IsValidIndex(blockIndex) || !loadedAnimation.IsValid())
    {
        return;
    }
    const auto& block = wordingBlocks[blockIndex];
    const double rate = loadedAnimation->GetSamplingFrameRate().AsDecimal();
    const double picked = FMath::Clamp((time - block.start) / block.duration, 0.0, 1.0);
    const int32 markerIndex = block.markers.IndexOfByPredicate([&](const FVisemeMarker& marker)
    {
        return FMath::Abs(marker.position - picked) * block.duration <= 0.55 / rate;
    });

    FVisemeMarker marker;
    if (block.markers.IsValidIndex(markerIndex))
    {
        marker = block.markers[markerIndex];
    }
    else
    {
        marker.position = picked;
    }

    const auto window = wordingWindow(TEXT("Mark Viseme"), FVector2D(520, 310));
    wordingBlockWindow = window;
    const TWeakPtr<SWindow> weakWindow = window;
    const auto weak = SharedThis(this).ToWeakPtr();
    const auto multiplier = SNew(SEditableTextBox).Text(FText::FromString(FString::SanitizeFloat(marker.multiplier)));
    const auto attack = SNew(SEditableTextBox).Text(FText::FromString(FString::SanitizeFloat(marker.attack)));
    const auto hold = SNew(SEditableTextBox).Text(FText::FromString(FString::SanitizeFloat(marker.hold)));
    const auto release = SNew(SEditableTextBox).Text(FText::FromString(FString::SanitizeFloat(marker.release)));
    const auto error = SNew(STextBlock).AutoWrapText(true);
    const auto body = SNew(SVerticalBox);
    const int32 frame = FMath::RoundToInt((block.start + marker.position * block.duration) * rate);
    body->AddSlot().AutoHeight().Padding(12, 8)
    [
        SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Frame %d · %s"), frame, *block.text)))
    ];
    const TCHAR* labels[] = {TEXT("Multiplier (1 = original, 2 = double)"), TEXT("Blend in before marker (seconds)"),
        TEXT("Hold from marker (seconds)"), TEXT("Blend out after hold (seconds)")};
    const TSharedRef<SEditableTextBox> fields[] = {multiplier, attack, hold, release};
    for (int32 index = 0; index < UE_ARRAY_COUNT(labels); ++index)
    {
        body->AddSlot().AutoHeight().Padding(12, 3)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(2)[SNew(STextBlock).Text(FText::FromString(labels[index]))]
            + SHorizontalBox::Slot().FillWidth(1)[fields[index]]
        ];
    }
    body->AddSlot().AutoHeight().Padding(12, 6)
    [
        SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(
            TEXT("The multiplier ramps in and out over the spoken region. Weights are clamped to −1 through 1.")))
    ];
    body->AddSlot().AutoHeight().Padding(12, 3)[error];
    body->AddSlot().AutoHeight().Padding(12, 4)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
        [
            SNew(SButton).Text(FText::FromString(TEXT("Apply")))
            .OnClicked_Lambda([weak, weakWindow, blockIndex, markerIndex, marker, multiplier, attack, hold, release, error]
            {
                const auto self = weak.Pin();
                if (!self || !self->wordingBlocks.IsValidIndex(blockIndex))
                {
                    return FReply::Handled();
                }
                FVisemeMarker updated = marker;
                if (!LexTryParseString(updated.multiplier, *multiplier->GetText().ToString()) ||
                    !LexTryParseString(updated.attack, *attack->GetText().ToString()) ||
                    !LexTryParseString(updated.hold, *hold->GetText().ToString()) ||
                    !LexTryParseString(updated.release, *release->GetText().ToString()) ||
                    !FMath::IsFinite(updated.multiplier) || updated.multiplier < 0 ||
                    !FMath::IsFinite(updated.attack) || updated.attack < 0 ||
                    !FMath::IsFinite(updated.hold) || updated.hold < 0 ||
                    !FMath::IsFinite(updated.release) || updated.release < 0)
                {
                    error->SetText(FText::FromString(TEXT("Use a finite, non-negative multiplier and non-negative times.")));
                    return FReply::Handled();
                }
                auto& block = self->wordingBlocks[blockIndex];
                if (block.markers.IsValidIndex(markerIndex))
                {
                    block.markers[markerIndex] = MoveTemp(updated);
                }
                else
                {
                    block.markers.Add(MoveTemp(updated));
                }
                block.markers.Sort([](const auto& a, const auto& b) { return a.position < b.position; });
                self->refreshWording();
                self->rebuildPreviewAnimation(true);
                if (const auto dialog = weakWindow.Pin()) { dialog->RequestDestroyWindow(); }
                return FReply::Handled();
            })
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SButton).Text(FText::FromString(TEXT("Remove Marker")))
            .Visibility(markerIndex == INDEX_NONE ? EVisibility::Collapsed : EVisibility::Visible)
            .OnClicked_Lambda([weak, weakWindow, blockIndex, markerIndex]
            {
                if (const auto self = weak.Pin(); self && self->wordingBlocks.IsValidIndex(blockIndex))
                {
                    auto& markers = self->wordingBlocks[blockIndex].markers;
                    if (markers.IsValidIndex(markerIndex))
                    {
                        markers.RemoveAt(markerIndex);
                        self->refreshWording();
                        self->rebuildPreviewAnimation(true);
                    }
                }
                if (const auto dialog = weakWindow.Pin()) { dialog->RequestDestroyWindow(); }
                return FReply::Handled();
            })
        ]
    ];
    window->SetContent(body);
    FSlateApplication::Get().AddWindow(window);
}
