#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "HandposeAsset.generated.h"

class UAnimSequence;
class UTexture2D;

UENUM(BlueprintType)
enum class EHandposeSide : uint8
{
    Left UMETA(DisplayName = "Left"),
    Right UMETA(DisplayName = "Right")
};

UCLASS(BlueprintType)
class HANDPOSETIMELINE_API UHandposeAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handpose")
    TSoftObjectPtr<UAnimSequence> predictionHandpose;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handpose")
    EHandposeSide handSide = EHandposeSide::Right;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handpose")
    TSoftObjectPtr<UAnimSequence> postProcessedHandpose;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handpose")
    TSoftObjectPtr<UTexture2D> thumbnail;

    bool isValid() const;
};
