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
    UObject* animation = candidate.IsValid()
        ? candidate->handshapeAnimation.LoadSynchronous()
        : nullptr;

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

    if (animation != nullptr && arguments._ThumbnailPool.IsValid())
    {
        assetThumbnail = MakeShared<FAssetThumbnail>(
            animation,
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
