#pragma once

#include "HandposeTimelineSegment.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnailPool;
class SHandshapeTileWidget;
class SSearchBox;
class STextBlock;
class SWindow;
template<typename ItemType> class STileView;
struct FHandshapeCandidate;

DECLARE_DELEGATE_TwoParams(FOnHandshapeChosen, int32, const FHandshapeCandidate&)

class SHandshapePickerWidget : public SCompoundWidget
{
public:
    using FCandidateItem = TSharedPtr<FHandshapeCandidate>;

    SLATE_BEGIN_ARGS(SHandshapePickerWidget)
        : _SegmentIndex(INDEX_NONE)
    {
    }
        SLATE_ARGUMENT(int32, SegmentIndex)
        SLATE_ARGUMENT(FHandposeTimelineSegment, Segment)
        SLATE_ARGUMENT(TArray<FHandshapeCandidate>, Candidates)
        SLATE_ARGUMENT(FString, EmptyMessage)
        SLATE_EVENT(FOnHandshapeChosen, OnHandshapeChosen)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);
    void setOwnerWindow(const TSharedRef<SWindow>& window);

private:
    void onSearchTextChanged(const FText& searchText);
    TSharedRef<ITableRow> onGenerateTile(
        FCandidateItem item,
        const TSharedRef<STableViewBase>& ownerTable);
    void onSelectionChanged(FCandidateItem item, ESelectInfo::Type selectInfo);
    FReply onCancelClicked();
    FReply onUseHandshapeClicked();
    FText getSelectedNameText() const;
    FText getSelectedRelevanceText() const;
    EVisibility getEmptyStateVisibility() const;
    bool canUseHandshape() const;
    void filterCandidates();

    int32 segmentIndex = INDEX_NONE;
    FHandposeTimelineSegment segment;
    FString searchText;
    FString emptyMessage;
    TArray<FCandidateItem> allCandidates;
    TArray<FCandidateItem> filteredCandidates;
    FCandidateItem selectedCandidate;
    FOnHandshapeChosen onHandshapeChosen;
    TWeakPtr<SWindow> ownerWindow;
    TSharedPtr<FAssetThumbnailPool> thumbnailPool;
    TSharedPtr<STileView<FCandidateItem>> tileView;
};
