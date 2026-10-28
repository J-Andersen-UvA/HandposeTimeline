#include "HandshapeCandidate.h"

bool FHandshapeCandidate::isValid() const
{
    return !handshapeId.IsNone() && !displayName.IsEmpty();
}

int32 FHandshapeCandidate::getRelevancePercent() const
{
    return FMath::RoundToInt(FMath::Clamp(relevanceScore, 0.0, 1.0) * 100.0);
}

bool FHandshapeCandidate::matchesSearch(const FString& searchText) const
{
    FString normalizedSearchText = searchText;
    normalizedSearchText.TrimStartAndEndInline();

    return normalizedSearchText.IsEmpty()
        || displayName.ToString().Contains(normalizedSearchText, ESearchCase::IgnoreCase)
        || handshapeId.ToString().Contains(normalizedSearchText, ESearchCase::IgnoreCase);
}

bool FHandshapeCandidate::isMoreRelevantThan(const FHandshapeCandidate& other) const
{
    return relevanceScore > other.relevanceScore;
}

void FHandshapeCandidate::sortByRelevance(TArray<FHandshapeCandidate>& candidates)
{
    candidates.StableSort(
        [](const FHandshapeCandidate& left, const FHandshapeCandidate& right)
        {
            return left.isMoreRelevantThan(right);
        });
}
