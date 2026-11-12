#include "Widgets/SHandshapeTileWidget.h"

#include "AssetThumbnail.h"
#include "HandshapeCandidate.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHandshapeTileWidget"

void SHandshapeTileWidget::Construct(const FArguments& arguments)
{
    candidate = arguments._Candidate;
    UObject* previewAsset = nullptr;
    if (candidate.IsValid())
    {
        previewAsset = candidate->previewTexture.LoadSynchronous();
        if (previewAsset == nullptr)
        {
            previewAsset = candidate->handshapeAnimation.LoadSynchronous();
        }
    }

    TSharedRef<SWidget> previewWidget =
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("NoPreview", "No Preview"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];

    if (previewAsset != nullptr && arguments._ThumbnailPool.IsValid())
    {
        assetThumbnail = MakeShared<FAssetThumbnail>(
            previewAsset,
            128,
            128,
            arguments._ThumbnailPool);
        FAssetThumbnailConfig thumbnailConfig;
        thumbnailConfig.bAllowFadeIn = true;
        thumbnailConfig.bAllowHintText = true;
        thumbnailConfig.bAllowRealTimeOnHovered = false;
        thumbnailConfig.AllowAssetSpecificThumbnailOverlay = false;
        thumbnailConfig.AllowAssetSpecificThumbnailOverlayIndicator = false;
        previewWidget = assetThumbnail->MakeThumbnailWidget(thumbnailConfig);
    }

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(150.0f)
        .HeightOverride(184.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .WidthOverride(128.0f)
                .HeightOverride(128.0f)
                .RenderTransform(candidate.IsValid() && candidate->mirrorToOppositeHand
                    ? FSlateRenderTransform(FScale2D(-1.0f, 1.0f))
                    : FSlateRenderTransform())
                .RenderTransformPivot(FVector2D(0.5f, 0.5f))
                [
                    previewWidget
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f, 5.0f, 2.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(candidate.IsValid() ? candidate->displayName : FText::GetEmpty())
                .Justification(ETextJustify::Center)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(2.0f, 2.0f, 2.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(candidate.IsValid()
                    ? FText::Format(
                        LOCTEXT("Relevance", "{0}% relevant"),
                        FText::AsNumber(candidate->getRelevancePercent()))
                    : FText::GetEmpty())
                .Justification(ETextJustify::Center)
                .Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]
        ]
    ];
}

#undef LOCTEXT_NAMESPACE
