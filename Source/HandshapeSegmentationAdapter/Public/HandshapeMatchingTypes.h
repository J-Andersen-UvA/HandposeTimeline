#pragma once

#include "CoreMinimal.h"
#include "HandshapeMatchingTypes.generated.h"

class UAnimSequenceBase;

UENUM(BlueprintType)
enum class EHandshapeMatchingMethod : uint8
{
    Euclidean,
    Cosine,
    Manhattan
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeMatch
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    FName handshapeId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    TObjectPtr<UAnimSequenceBase> handshapeAnimation = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching", meta = (ClampMin = "0.0"))
    double distance = 0.0;
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeMatchingResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    bool success = false;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    FString errorMessage;

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    TArray<FHandshapeMatch> matches;
};
