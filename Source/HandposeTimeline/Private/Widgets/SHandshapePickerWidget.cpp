#include "Widgets/SHandshapePickerWidget.h"

#include "AssetThumbnail.h"
#include "HandshapeCandidate.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SHandshapeTileWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STileView.h"

#define LOCTEXT_NAMESPACE "SHandshapePickerWidget"

void SHandshapePickerWidget::Construct(const FArguments& arguments)
{
    segmentIndex = arguments._SegmentIndex;
    segment = arguments._Segment;
    emptyMessage = arguments._EmptyMessage;
    onHandshapeChosen = arguments._OnHandshapeChosen;
    thumbnailPool = MakeShared<FAssetThumbnailPool>(64);

    for (const FHandshapeCandidate& candidate : arguments._Candidates)
    {
        allCandidates.Add(MakeShared<FHandshapeCandidate>(candidate));
    }
    filterCandidates();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(12.0f)
        .IsEnabled(segmentIndex != INDEX_NONE && segment.isValid())
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(segmentIndex != INDEX_NONE
                    ? FText::Format(
                        LOCTEXT("EditingSegment", "Editing Segment {0} · Frames {1}-{2}"),
                        FText::AsNumber(segmentIndex + 1),
                        FText::AsNumber(segment.startFrame),
                        FText::AsNumber(segment.endFrame))
                    : LOCTEXT("NoSegment", "No segment selected"))
                .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f)
            [
                SNew(SSearchBox)
                .HintText(LOCTEXT("SearchHint", "Search handshapes"))
                .OnTextChanged(this, &SHandshapePickerWidget::onSearchTextChanged)
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SAssignNew(tileView, STileView<FCandidateItem>)
                    .ListItemsSource(&filteredCandidates)
                    .OnGenerateTile(this, &SHandshapePickerWidget::onGenerateTile)
                    .OnSelectionChanged(this, &SHandshapePickerWidget::onSelectionChanged)
                    .ItemWidth(160.0f)
                    .ItemHeight(194.0f)
                    .SelectionMode(ESelectionMode::Single)
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Visibility(this, &SHandshapePickerWidget::getEmptyStateVisibility)
                    .Text_Lambda(
                        [this]()
                        {
                            return FText::FromString(
                                emptyMessage.IsEmpty()
                                    ? TEXT("No handshapes match the search.")
                                    : emptyMessage);
                        })
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                SNew(SSeparator)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(this, &SHandshapePickerWidget::getSelectedNameText)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(this, &SHandshapePickerWidget::getSelectedRelevanceText)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Bottom)
                [
                    SNew(SUniformGridPanel)
                    .SlotPadding(FMargin(4.0f, 0.0f))
                    + SUniformGridPanel::Slot(0, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("Cancel", "Cancel"))
                        .OnClicked(this, &SHandshapePickerWidget::onCancelClicked)
                    ]
                    + SUniformGridPanel::Slot(1, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("UseHandshape", "Use Handshape"))
                        .IsEnabled(this, &SHandshapePickerWidget::canUseHandshape)
                        .OnClicked(this, &SHandshapePickerWidget::onUseHandshapeClicked)
                    ]
                ]
            ]
        ]
    ];

    if (!segment.selectedHandshapeId.IsNone())
    {
        const FCandidateItem* existingSelection = allCandidates.FindByPredicate(
            [this](const FCandidateItem& candidate)
            {
                return candidate.IsValid() &&
                    candidate->handshapeId == segment.selectedHandshapeId;
            });
        if (existingSelection != nullptr)
        {
            selectedCandidate = *existingSelection;
            tileView->SetSelection(selectedCandidate);
        }
    }
}

void SHandshapePickerWidget::setOwnerWindow(const TSharedRef<SWindow>& window)
{
    ownerWindow = window;
}

void SHandshapePickerWidget::onSearchTextChanged(const FText& newSearchText)
{
    searchText = newSearchText.ToString();
    emptyMessage.Reset();
    filterCandidates();
    tileView->RequestListRefresh();
}

TSharedRef<ITableRow> SHandshapePickerWidget::onGenerateTile(
    FCandidateItem item,
    const TSharedRef<STableViewBase>& ownerTable)
{
    return SNew(STableRow<FCandidateItem>, ownerTable)
        .Padding(4.0f)
        [
            SNew(SHandshapeTileWidget)
            .Candidate(item)
            .ThumbnailPool(thumbnailPool)
        ];
}

void SHandshapePickerWidget::onSelectionChanged(
    FCandidateItem item,
    ESelectInfo::Type selectInfo)
{
    selectedCandidate = item;
}

FReply SHandshapePickerWidget::onCancelClicked()
{
    if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FReply SHandshapePickerWidget::onUseHandshapeClicked()
{
    if (selectedCandidate.IsValid())
    {
        onHandshapeChosen.ExecuteIfBound(segmentIndex, selectedCandidate->handshapeId);
    }
    if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
    {
        window->RequestDestroyWindow();
    }
    return FReply::Handled();
}

FText SHandshapePickerWidget::getSelectedNameText() const
{
    return selectedCandidate.IsValid()
        ? selectedCandidate->displayName
        : LOCTEXT("NothingSelected", "No handshape selected");
}

FText SHandshapePickerWidget::getSelectedRelevanceText() const
{
    return selectedCandidate.IsValid()
        ? FText::Format(
            LOCTEXT("SelectedRelevance", "Relevance: {0}%"),
            FText::AsNumber(selectedCandidate->getRelevancePercent()))
        : FText::GetEmpty();
}

EVisibility SHandshapePickerWidget::getEmptyStateVisibility() const
{
    return filteredCandidates.IsEmpty()
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

bool SHandshapePickerWidget::canUseHandshape() const
{
    return selectedCandidate.IsValid();
}

void SHandshapePickerWidget::filterCandidates()
{
    filteredCandidates.Reset();
    for (const FCandidateItem& candidate : allCandidates)
    {
        if (candidate.IsValid() && candidate->matchesSearch(searchText))
        {
            filteredCandidates.Add(candidate);
        }
    }
}

#undef LOCTEXT_NAMESPACE
