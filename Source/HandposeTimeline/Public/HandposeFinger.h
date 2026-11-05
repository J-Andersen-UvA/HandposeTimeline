#pragma once

#include "CoreMinimal.h"
#include "HandposeFinger.generated.h"

UENUM(BlueprintType)
enum class EHandposeFinger : uint8
{
    Thumb,
    Index,
    Middle,
    Ring,
    Pinky
};
