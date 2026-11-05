#include "HandposeDecisionDataService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr int32 supportedSchemaVersion = 1;
const TCHAR* decisionAssetFolder = TEXT("/Game/HandposeTimeline/DecisionData");

FString getAssetName(const FHandposeDecisionSessionData& sessionData)
{
    return FString::Printf(
        TEXT("%s_DecisionData"),
        *ObjectTools::SanitizeObjectName(sessionData.sourceAnimationName));
}
}

FHandposeDecisionDataResult FHandposeDecisionDataService::capture(
    const FHandposeDecisionSessionData& sessionData)
{
    const FString jsonDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("HandposeTimeline"),
        TEXT("DecisionData"));
    const FString jsonPath = FPaths::Combine(
        jsonDirectory,
        getAssetName(sessionData) + TEXT(".json"));
    return save(sessionData, jsonPath);
}

FHandposeDecisionDataResult FHandposeDecisionDataService::importJson(
    const FString& jsonPath)
{
    FHandposeDecisionDataResult result;
    FString json;
    if (!FFileHelper::LoadFileToString(json, *jsonPath))
    {
        result.errorMessage = FString::Printf(TEXT("Could not read %s."), *jsonPath);
        return result;
    }

    FHandposeDecisionSessionData sessionData;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(json, &sessionData))
    {
        result.errorMessage = TEXT("The selected file is not valid HandposeTimeline decision data.");
        return result;
    }
    return save(sessionData, jsonPath);
}

bool FHandposeDecisionDataService::validate(
    const FHandposeDecisionSessionData& sessionData,
    FString& errorMessage)
{
    if (sessionData.schemaVersion != supportedSchemaVersion)
    {
        errorMessage = FString::Printf(
            TEXT("Unsupported decision-data schema version %d."),
            sessionData.schemaVersion);
        return false;
    }
    if (sessionData.sourceAnimationName.IsEmpty())
    {
        errorMessage = TEXT("Decision data requires a source animation name.");
        return false;
    }
    if (sessionData.totalFrames <= 0 || sessionData.framesPerSecond <= 0.0)
    {
        errorMessage = TEXT("Decision data has an invalid frame count or frame rate.");
        return false;
    }
    for (const FHandposeDecisionSegmentSnapshot& segment : sessionData.segments)
    {
        if ((segment.handSide != TEXT("Left") && segment.handSide != TEXT("Right")) ||
            segment.segmentIndex < 0 || segment.startFrame < 0 ||
            segment.endFrame < segment.startFrame || segment.endFrame >= sessionData.totalFrames)
        {
            errorMessage = TEXT("Decision data contains an invalid segment range.");
            return false;
        }
        if (segment.topCandidates.Num() > 10)
        {
            errorMessage = TEXT("Decision data contains more than 10 candidates for a segment.");
            return false;
        }
    }
    for (const FHandposeDecisionAssignmentSnapshot& assignment : sessionData.assignments)
    {
        if ((assignment.handSide != TEXT("Left") && assignment.handSide != TEXT("Right")) ||
            assignment.assignmentId.IsEmpty() || assignment.handposeId.IsEmpty() ||
            assignment.blendStartFrame < 0 ||
            assignment.fullPoseStartFrame < assignment.blendStartFrame ||
            assignment.fullPoseEndFrame < assignment.fullPoseStartFrame ||
            assignment.blendEndFrame < assignment.fullPoseEndFrame ||
            assignment.blendEndFrame >= sessionData.totalFrames)
        {
            errorMessage = TEXT("Decision data contains an invalid assignment.");
            return false;
        }
        const bool hasSourceSegment = sessionData.segments.ContainsByPredicate(
            [&assignment](const FHandposeDecisionSegmentSnapshot& segment)
            {
                return segment.handSide == assignment.handSide &&
                    segment.segmentIndex == assignment.sourceSegmentIndex;
            });
        if (!hasSourceSegment)
        {
            errorMessage = TEXT("Decision data contains an assignment with no source segment.");
            return false;
        }
        for (const int32 coveredSegmentIndex : assignment.coveredSegmentIndices)
        {
            const bool hasCoveredSegment = sessionData.segments.ContainsByPredicate(
                [&assignment, coveredSegmentIndex](
                    const FHandposeDecisionSegmentSnapshot& segment)
                {
                    return segment.handSide == assignment.handSide &&
                        segment.segmentIndex == coveredSegmentIndex;
                });
            if (!hasCoveredSegment)
            {
                errorMessage = TEXT("Decision data references a covered segment that does not exist.");
                return false;
            }
        }
    }
    return true;
}

FHandposeDecisionDataResult FHandposeDecisionDataService::save(
    const FHandposeDecisionSessionData& sessionData,
    const FString& jsonPath)
{
    FHandposeDecisionDataResult result;
    if (!validate(sessionData, result.errorMessage))
    {
        return result;
    }

    FString json;
    if (!FJsonObjectConverter::UStructToJsonObjectString(sessionData, json))
    {
        result.errorMessage = TEXT("Could not serialize the decision data to JSON.");
        return result;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(jsonPath), true);
    if (!FFileHelper::SaveStringToFile(json, *jsonPath))
    {
        result.errorMessage = FString::Printf(TEXT("Could not write %s."), *jsonPath);
        return result;
    }

    const FString assetName = getAssetName(sessionData);
    const FString packageName = FString::Printf(
        TEXT("%s/%s"),
        decisionAssetFolder,
        *assetName);
    UPackage* package = FindPackage(nullptr, *packageName);
    if (package == nullptr && FPackageName::DoesPackageExist(packageName))
    {
        package = LoadPackage(nullptr, *packageName, LOAD_None);
    }
    if (package == nullptr)
    {
        package = CreatePackage(*packageName);
    }
    UHandposeDecisionSession* decisionSession = FindObject<UHandposeDecisionSession>(
        package,
        *assetName);
    const bool isNewAsset = decisionSession == nullptr;
    if (isNewAsset)
    {
        decisionSession = NewObject<UHandposeDecisionSession>(
            package,
            *assetName,
            RF_Public | RF_Standalone | RF_Transactional);
    }
    decisionSession->Modify();
    decisionSession->sessionData = sessionData;
    decisionSession->MarkPackageDirty();
    if (isNewAsset)
    {
        FAssetRegistryModule::AssetCreated(decisionSession);
    }

    const FString packageFilename = FPackageName::LongPackageNameToFilename(
        packageName,
        FPackageName::GetAssetPackageExtension());
    FSavePackageArgs saveArgs;
    saveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    saveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(package, decisionSession, *packageFilename, saveArgs))
    {
        result.errorMessage = FString::Printf(TEXT("Could not save %s."), *packageName);
        return result;
    }

    result.success = true;
    result.assetPath = packageName;
    result.jsonPath = jsonPath;
    return result;
}
