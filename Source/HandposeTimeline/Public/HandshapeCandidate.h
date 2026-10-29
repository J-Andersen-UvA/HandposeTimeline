#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "HandshapeCandidate.generated.h"

class UTexture2D;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct HANDPOSETIMELINE_API FHandshapeCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    FName handshapeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    TSoftObjectPtr<UTexture2D> previewTexture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline")
    TSoftObjectPtr<UAnimSequenceBase> handshapeAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handpose Timeline", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    double relevanceScore = 0.0;

    bool isValid() const;
    int32 getRelevancePercent() const;
    bool matchesSearch(const FString& searchText) const;
    bool isMoreRelevantThan(const FHandshapeCandidate& other) const;

    static void sortByRelevance(TArray<FHandshapeCandidate>& candidates);
};
