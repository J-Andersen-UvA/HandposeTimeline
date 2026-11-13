#include "AnimationPlacementService.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimPose.h"
#include "BoneCorrectionEditorLibrary.h"
#include "BoneCorrectionTypes.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "AnimationPlacementService"

namespace
{
FAnimPoseEvaluationOptions makeEvaluationOptions(const USkeletalMesh* skeletalMesh)
{
    FAnimPoseEvaluationOptions options;
    options.EvaluationType = EAnimDataEvalType::Raw;
    options.bShouldRetarget = true;
    options.bEvaluateCurves = false;
    options.OptionalSkeletalMesh = const_cast<USkeletalMesh*>(skeletalMesh);
    return options;
}

bool evaluatePose(
    UAnimSequence& animation,
    const USkeletalMesh& skeletalMesh,
    const int32 frame,
    FAnimPose& pose,
    FString& errorMessage)
{
    const FFrameRate frameRate = animation.GetSamplingFrameRate();
    if (!frameRate.IsValid() || frame < 0 ||
        frame >= animation.GetNumberOfSampledKeys())
    {
        errorMessage = TEXT("The trim-start frame is invalid.");
        return false;
    }
    UAnimPoseExtensions::GetAnimPoseAtTime(
        &animation,
        frameRate.AsSeconds(FFrameTime(frame)),
        makeEvaluationOptions(&skeletalMesh),
        pose);
    if (!UAnimPoseExtensions::IsValid(pose))
    {
        errorMessage = TEXT("Unreal could not evaluate the trim-start pose.");
        return false;
    }
    return true;
}

FTransform getReferenceComponentTransform(
    const FReferenceSkeleton& referenceSkeleton,
    const int32 boneIndex)
{
    FTransform transform = referenceSkeleton.GetRefBonePose()[boneIndex];
    int32 parentIndex = referenceSkeleton.GetParentIndex(boneIndex);
    while (parentIndex != INDEX_NONE)
    {
        transform *= referenceSkeleton.GetRefBonePose()[parentIndex];
        parentIndex = referenceSkeleton.GetParentIndex(parentIndex);
    }
    return transform;
}

const FBoneCorrectionBoneTransform* findSolvedBone(
    const TArray<FBoneCorrectionBoneTransform>& transforms,
    const FName boneName)
{
    return transforms.FindByPredicate(
        [boneName](const FBoneCorrectionBoneTransform& transform)
        {
            return transform.boneName == boneName;
        });
}

void addOrReplaceDelta(
    TArray<FBoneCorrectionBoneDelta>& deltas,
    const FBoneCorrectionBoneDelta& value)
{
    if (FBoneCorrectionBoneDelta* existing = deltas.FindByPredicate(
            [&value](const FBoneCorrectionBoneDelta& delta)
            {
                return delta.boneName == value.boneName;
            }))
    {
        *existing = value;
        return;
    }
    deltas.Add(value);
}

bool solveLeg(
    UAnimSequence& animation,
    const USkeletalMesh& skeletalMesh,
    const FAnimPose& pose,
    const FAnimationPlacementLegChain& leg,
    const int32 frame,
    const FVector& planePoint,
    const FVector& planeNormal,
    TArray<FBoneCorrectionBoneDelta>& deltas,
    FString& errorMessage)
{
    if (leg.bones.Num() < 3 || leg.toeBone.IsNone())
    {
        errorMessage = TEXT("Each leg needs an ordered upper-leg, lower-leg, foot chain and a toe bone.");
        return false;
    }

    const FName footBone = leg.bones.Last();
    const FReferenceSkeleton& referenceSkeleton = skeletalMesh.GetRefSkeleton();
    const int32 footIndex = referenceSkeleton.FindBoneIndex(footBone);
    const int32 toeIndex = referenceSkeleton.FindBoneIndex(leg.toeBone);
    if (footIndex == INDEX_NONE || toeIndex == INDEX_NONE)
    {
        errorMessage = FString::Printf(
            TEXT("Foot bone '%s' or toe bone '%s' does not exist on the skeletal mesh."),
            *footBone.ToString(),
            *leg.toeBone.ToString());
        return false;
    }
    const int32 expectedFootParent = referenceSkeleton.FindBoneIndex(
        leg.bones[leg.bones.Num() - 2]);
    if (referenceSkeleton.GetParentIndex(footIndex) != expectedFootParent)
    {
        errorMessage = FString::Printf(
            TEXT("The IK chain is not contiguous before foot bone '%s'."),
            *footBone.ToString());
        return false;
    }

    const FTransform footTransform = UAnimPoseExtensions::GetBonePose(
        pose,
        footBone,
        EAnimPoseSpaces::World);
    const FTransform toeTransform = UAnimPoseExtensions::GetBonePose(
        pose,
        leg.toeBone,
        EAnimPoseSpaces::World);
    const FTransform referenceFoot = getReferenceComponentTransform(
        referenceSkeleton,
        footIndex);
    const FTransform referenceToe = getReferenceComponentTransform(
        referenceSkeleton,
        toeIndex);

    const FVector referenceUpLocal = referenceFoot.GetRotation().Inverse()
        .RotateVector(FVector::UpVector)
        .GetSafeNormal();
    FVector referenceForwardLocal = referenceFoot.InverseTransformPosition(
        referenceToe.GetTranslation()).GetSafeNormal();
    if (referenceForwardLocal.IsNearlyZero())
    {
        errorMessage = FString::Printf(
            TEXT("Toe bone '%s' has no usable offset from foot bone '%s'."),
            *leg.toeBone.ToString(),
            *footBone.ToString());
        return false;
    }

    const FQuat currentFootRotation = footTransform.GetRotation();
    const FVector currentUp = currentFootRotation.RotateVector(referenceUpLocal);
    FQuat desiredFootRotation =
        (FQuat::FindBetweenNormals(currentUp, planeNormal) * currentFootRotation)
          .GetNormalized();
    FVector desiredForward = toeTransform.GetTranslation() -
        footTransform.GetTranslation();
    desiredForward = FVector::VectorPlaneProject(desiredForward, planeNormal)
        .GetSafeNormal();
    FVector flattenedForward = FVector::VectorPlaneProject(
        desiredFootRotation.RotateVector(referenceForwardLocal),
        planeNormal).GetSafeNormal();
    if (!desiredForward.IsNearlyZero() && !flattenedForward.IsNearlyZero())
    {
        const double yaw = FMath::Atan2(
            FVector::DotProduct(
                planeNormal,
                FVector::CrossProduct(flattenedForward, desiredForward)),
            FVector::DotProduct(flattenedForward, desiredForward));
        desiredFootRotation =
            (FQuat(planeNormal, yaw) * desiredFootRotation).GetNormalized();
    }

    const FVector toeLocal = footTransform.InverseTransformPosition(
        toeTransform.GetTranslation());
    const FVector desiredToe = footTransform.GetTranslation() +
        desiredFootRotation.RotateVector(toeLocal);
    const double toeDistance = FVector::DotProduct(
        desiredToe - planePoint,
        planeNormal);
    const FVector desiredFootPosition =
        footTransform.GetTranslation() - planeNormal * toeDistance;

    FBoneCorrectionChain positionChain;
    positionChain.bones = leg.bones;
    positionChain.bones.Pop();
    FBoneCorrectionIkSettings ikSettings;
    const FBoneCorrectionIkResult solved =
        UBoneCorrectionEditorLibrary::solvePositionIkAtFrame(
            &animation,
            &skeletalMesh,
            frame,
            footBone,
            FVector::ZeroVector,
            desiredFootPosition,
            positionChain,
            ikSettings);
    if (!solved.isValid)
    {
        errorMessage = FString::Printf(
            TEXT("Could not place foot '%s': %s"),
            *footBone.ToString(),
            *solved.errorMessage);
        return false;
    }
    for (const FBoneCorrectionBoneDelta& delta : solved.correctionKey.boneDeltas)
    {
        FBoneCorrectionBoneDelta rotationOnlyDelta = delta;
        rotationOnlyDelta.translationDelta = FVector::ZeroVector;
        rotationOnlyDelta.scaleMultiplier = FVector::OneVector;
        addOrReplaceDelta(deltas, rotationOnlyDelta);
    }

    const int32 footParentIndex = referenceSkeleton.GetParentIndex(footIndex);
    const FName footParentName = referenceSkeleton.GetBoneName(footParentIndex);
    const FBoneCorrectionBoneTransform* solvedParent = findSolvedBone(
        solved.solvedBoneTransforms,
        footParentName);
    const FTransform parentTransform = solvedParent != nullptr
        ? solvedParent->componentSpaceTransform
        : UAnimPoseExtensions::GetBonePose(
            pose,
            footParentName,
            EAnimPoseSpaces::World);
    const FTransform desiredFootTransform(
        desiredFootRotation,
        desiredFootPosition,
        footTransform.GetScale3D());
    const FTransform desiredFootLocal = desiredFootTransform.GetRelativeTransform(
        parentTransform);
    const FTransform baseFootLocal = UAnimPoseExtensions::GetBonePose(
        pose,
        footBone,
        EAnimPoseSpaces::Local);
    FBoneCorrectionBoneDelta footDelta;
    footDelta.boneName = footBone;
    footDelta.translationDelta = FVector::ZeroVector;
    footDelta.rotationDelta =
        (desiredFootLocal.GetRotation() * baseFootLocal.GetRotation().Inverse())
          .GetNormalized();
    addOrReplaceDelta(deltas, footDelta);
    return true;
}

const FBoneCorrectionBoneDelta* findDelta(
    const TArray<FBoneCorrectionBoneDelta>& deltas,
    const FName boneName)
{
    return deltas.FindByPredicate(
        [boneName](const FBoneCorrectionBoneDelta& delta)
        {
            return delta.boneName == boneName;
        });
}
}

FAnimationPlacementResult FAnimationPlacementService::rotate180(
    UAnimSequence* sourceAnimation)
{
    FAnimationPlacementResult result;
    if (sourceAnimation == nullptr || sourceAnimation->GetSkeleton() == nullptr)
    {
        result.errorMessage = TEXT("A source animation with a skeleton is required.");
        return result;
    }

    const FReferenceSkeleton& referenceSkeleton =
        sourceAnimation->GetSkeleton()->GetReferenceSkeleton();
    if (referenceSkeleton.GetNum() == 0)
    {
        result.errorMessage = TEXT("The source animation skeleton has no bones.");
        return result;
    }

    UAnimSequence* rotatedAnimation = DuplicateObject<UAnimSequence>(
        sourceAnimation,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UAnimSequence::StaticClass(),
            TEXT("HandposeTimelineRotate180")));
    if (rotatedAnimation == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the rotated preview animation.");
        return result;
    }
    rotatedAnimation->ClearFlags(RF_Public | RF_Standalone);
    rotatedAnimation->SetFlags(RF_Transient);

    const FName rootBoneName = referenceSkeleton.GetBoneName(0);
    IAnimationDataModel* dataModel = rotatedAnimation->GetDataModel();
    IAnimationDataController& controller = rotatedAnimation->GetController();
    const bool hasRootTrack = dataModel->IsValidBoneTrackName(rootBoneName);

    TArray<FTransform> rootTransforms;
    if (hasRootTrack)
    {
        dataModel->GetBoneTrackTransforms(rootBoneName, rootTransforms);
    }
    else
    {
        rootTransforms.Init(
            referenceSkeleton.GetRefBonePose()[0],
            rotatedAnimation->GetNumberOfSampledKeys());
    }
    if (rootTransforms.IsEmpty())
    {
        result.errorMessage = TEXT("The source animation has no root-bone samples.");
        return result;
    }

    const FQuat rotationOffset(FVector::UpVector, PI);
    TArray<FVector> translations;
    TArray<FQuat> rotations;
    TArray<FVector> scales;
    translations.Reserve(rootTransforms.Num());
    rotations.Reserve(rootTransforms.Num());
    scales.Reserve(rootTransforms.Num());
    for (const FTransform& rootTransform : rootTransforms)
    {
        translations.Add(rotationOffset.RotateVector(rootTransform.GetTranslation()));
        rotations.Add(
            (rotationOffset * rootTransform.GetRotation()).GetNormalized());
        scales.Add(rootTransform.GetScale3D());
    }

    IAnimationDataController::FScopedBracket bracket(
        controller,
        LOCTEXT("RotateAnimation180", "Rotate animation 180 degrees"),
        false);
    if (!hasRootTrack && !controller.AddBoneCurve(rootBoneName, false))
    {
        result.errorMessage = FString::Printf(
            TEXT("Could not add the root track '%s'."),
            *rootBoneName.ToString());
        return result;
    }
    if (!controller.SetBoneTrackKeys(
            rootBoneName,
            translations,
            rotations,
            scales,
            false))
    {
        result.errorMessage = FString::Printf(
            TEXT("Could not write the rotated root track '%s'."),
            *rootBoneName.ToString());
        return result;
    }

    rotatedAnimation->PostEditChange();
    result.success = true;
    result.animation = rotatedAnimation;
    return result;
}

FAnimationPlacementResult FAnimationPlacementService::placeFeetOnGround(
    UAnimSequence* sourceAnimation,
    const USkeletalMesh* skeletalMesh,
    const FAnimationGroundSettings& settings)
{
    FAnimationPlacementResult result;
    if (sourceAnimation == nullptr || skeletalMesh == nullptr)
    {
        result.errorMessage = TEXT("A source animation and skeletal mesh are required.");
        return result;
    }

    const FVector worldNormal = settings.planeWorldNormal.GetSafeNormal();
    if (worldNormal.IsNearlyZero())
    {
        result.errorMessage = TEXT("The ground plane has no valid normal.");
        return result;
    }
    const FVector planePoint = settings.skeletalMeshComponentTransform
        .InverseTransformPosition(settings.planeWorldPoint);
    const FVector planeNormal = settings.skeletalMeshComponentTransform
        .InverseTransformVectorNoScale(worldNormal).GetSafeNormal();

    FAnimPose pose;
    if (!evaluatePose(
            *sourceAnimation,
            *skeletalMesh,
            settings.solveFrame,
            pose,
            result.errorMessage))
    {
        return result;
    }

    TArray<FBoneCorrectionBoneDelta> deltas;
    if (!solveLeg(
            *sourceAnimation,
            *skeletalMesh,
            pose,
            settings.leftLeg,
            settings.solveFrame,
            planePoint,
            planeNormal,
            deltas,
            result.errorMessage) ||
        !solveLeg(
            *sourceAnimation,
            *skeletalMesh,
            pose,
            settings.rightLeg,
            settings.solveFrame,
            planePoint,
            planeNormal,
            deltas,
            result.errorMessage))
    {
        return result;
    }

    TArray<FName> correctedBones;
    for (const FBoneCorrectionBoneDelta& delta : deltas)
    {
        correctedBones.AddUnique(delta.boneName);
    }
    const int32 frameCount = sourceAnimation->GetNumberOfSampledKeys();
    const FFrameRate frameRate = sourceAnimation->GetSamplingFrameRate();
    TMap<FName, TArray<FVector>> translations;
    TMap<FName, TArray<FQuat>> rotations;
    TMap<FName, TArray<FVector>> scales;
    for (const FName boneName : correctedBones)
    {
        translations.Add(boneName).Reserve(frameCount);
        rotations.Add(boneName).Reserve(frameCount);
        scales.Add(boneName).Reserve(frameCount);
    }
    for (int32 frame = 0; frame < frameCount; ++frame)
    {
        FAnimPose framePose;
        UAnimPoseExtensions::GetAnimPoseAtTime(
            sourceAnimation,
            frameRate.AsSeconds(FFrameTime(frame)),
            makeEvaluationOptions(skeletalMesh),
            framePose);
        if (!UAnimPoseExtensions::IsValid(framePose))
        {
            result.errorMessage = TEXT("Unreal could not evaluate every animation frame.");
            return result;
        }
        for (const FName boneName : correctedBones)
        {
            FTransform localTransform = UAnimPoseExtensions::GetBonePose(
                framePose,
                boneName,
                EAnimPoseSpaces::Local);
            if (const FBoneCorrectionBoneDelta* delta = findDelta(deltas, boneName))
            {
                localTransform.SetRotation(
                    (delta->rotationDelta * localTransform.GetRotation())
                      .GetNormalized());
            }
            translations.FindChecked(boneName).Add(localTransform.GetTranslation());
            rotations.FindChecked(boneName).Add(localTransform.GetRotation());
            scales.FindChecked(boneName).Add(localTransform.GetScale3D());
        }
    }

    UAnimSequence* placedAnimation = DuplicateObject<UAnimSequence>(
        sourceAnimation,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UAnimSequence::StaticClass(),
            TEXT("HandposeTimelineGroundPlacement")));
    if (placedAnimation == nullptr)
    {
        result.errorMessage = TEXT("Unreal could not create the placement preview animation.");
        return result;
    }
    placedAnimation->ClearFlags(RF_Public | RF_Standalone);
    placedAnimation->SetFlags(RF_Transient);

    IAnimationDataModel* dataModel = placedAnimation->GetDataModel();
    IAnimationDataController& controller = placedAnimation->GetController();
    IAnimationDataController::FScopedBracket bracket(
        controller,
        LOCTEXT("PlaceAnimationOnGround", "Place animation feet on ground"),
        false);
    for (const FName boneName : correctedBones)
    {
        if (!dataModel->IsValidBoneTrackName(boneName) &&
            !controller.AddBoneCurve(boneName, false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not add placement track '%s'."),
                *boneName.ToString());
            return result;
        }
        if (!controller.SetBoneTrackKeys(
                boneName,
                translations.FindChecked(boneName),
                rotations.FindChecked(boneName),
                scales.FindChecked(boneName),
                false))
        {
            result.errorMessage = FString::Printf(
                TEXT("Could not write placement track '%s'."),
                *boneName.ToString());
            return result;
        }
    }
    placedAnimation->PostEditChange();
    result.success = true;
    result.animation = placedAnimation;
    return result;
}

#undef LOCTEXT_NAMESPACE
