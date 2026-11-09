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

UENUM(BlueprintType)
enum class EHandshapeDescriptorMethod : uint8
{
    JointRotations UMETA(DisplayName = "Joint Rotations"),
    HandSkeleton UMETA(DisplayName = "Hand Skeleton (Global Positions)")
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeThumbBoneMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName metacarpal = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName proximal = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName distal = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName tip = NAME_None;
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeFingerBoneMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName metacarpal = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName proximal = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName middle = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName distal = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FName tip = NAME_None;
};

USTRUCT(BlueprintType)
struct HANDSHAPESEGMENTATIONADAPTER_API FHandshapeBoneMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FHandshapeThumbBoneMapping thumb;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FHandshapeFingerBoneMapping index;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FHandshapeFingerBoneMapping middle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FHandshapeFingerBoneMapping ring;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Handshape Matching")
    FHandshapeFingerBoneMapping pinky;
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

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    TArray<double> descriptor;
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

    UPROPERTY(BlueprintReadOnly, Category = "Handshape Matching")
    TArray<double> inputDescriptor;
};
