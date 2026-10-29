#pragma once

#include "Widgets/SCompoundWidget.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
struct FHandshapeCandidate;

class SHandshapeTileWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHandshapeTileWidget)
    {
    }
        SLATE_ARGUMENT(TSharedPtr<FHandshapeCandidate>, Candidate)
        SLATE_ARGUMENT(TSharedPtr<FAssetThumbnailPool>, ThumbnailPool)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments);

private:
    TSharedPtr<FHandshapeCandidate> candidate;
    TSharedPtr<FAssetThumbnail> assetThumbnail;
};
