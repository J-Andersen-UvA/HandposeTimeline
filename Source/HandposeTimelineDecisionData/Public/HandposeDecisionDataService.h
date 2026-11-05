#pragma once

#include "CoreMinimal.h"
#include "HandposeDecisionSession.h"

struct HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionDataResult
{
    bool success = false;
    FString assetPath;
    FString jsonPath;
    FString errorMessage;
};

class HANDPOSETIMELINEDECISIONDATA_API FHandposeDecisionDataService
{
public:
    static FHandposeDecisionDataResult capture(
        const FHandposeDecisionSessionData& sessionData);

    static FHandposeDecisionDataResult importJson(const FString& jsonPath);

private:
    static bool validate(
        const FHandposeDecisionSessionData& sessionData,
        FString& errorMessage);
    static FHandposeDecisionDataResult save(
        const FHandposeDecisionSessionData& sessionData,
        const FString& jsonPath);
};
