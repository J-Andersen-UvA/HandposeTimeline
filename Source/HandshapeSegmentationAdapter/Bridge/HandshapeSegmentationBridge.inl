#include "HandshapeSegmentationBridge.h"

#include "matching/handshape.h"
#include "matching/handskeleton.h"
#include "lib/math/math.h"

#include <algorithm>

namespace
{
Pose toUpstreamPose(const handshapeSegmentationBridge::Pose& source)
{
    Pose pose;
    pose.joints.reserve(source.joints.size());
    for (const handshapeSegmentationBridge::Quaternion& quaternion : source.joints)
    {
        pose.joints.emplace_back(
            quaternion.w,
            quaternion.x,
            quaternion.y,
            quaternion.z);
    }
    return pose;
}

std::vector<Eigen::Quaterniond> toUpstreamRotations(
    const std::vector<handshapeSegmentationBridge::Quaternion>& source)
{
    std::vector<Eigen::Quaterniond> rotations;
    rotations.reserve(source.size());
    for (const handshapeSegmentationBridge::Quaternion& quaternion : source)
    {
        rotations.emplace_back(
            quaternion.w,
            quaternion.x,
            quaternion.y,
            quaternion.z);
    }
    return rotations;
}

MatchingMethod toUpstreamMatchingMethod(
    const handshapeSegmentationBridge::MatchingMethod method)
{
    switch (method)
    {
    case handshapeSegmentationBridge::MatchingMethod::Cosine:
        return MatchingMethod::Cosine;
    case handshapeSegmentationBridge::MatchingMethod::Manhattan:
        return MatchingMethod::Manhattan;
    case handshapeSegmentationBridge::MatchingMethod::Euclidean:
    default:
        return MatchingMethod::Euclidean;
    }
}

std::vector<double> toJointRotationDescriptor(const Pose& pose)
{
    std::vector<double> descriptor;
    descriptor.reserve(pose.joints.size());
    for (const Eigen::Quaterniond& rotation : pose.joints)
    {
        descriptor.push_back(
            rotation.toRotationMatrix().eulerAngles(0, 1, 2).x());
    }
    return descriptor;
}

double poseDistance(
    const Pose& left,
    const Pose& right,
    const MatchingMethod method)
{
    const std::vector<double> leftDescriptor =
        toJointRotationDescriptor(left);
    const std::vector<double> rightDescriptor =
        toJointRotationDescriptor(right);
    switch (method)
    {
    case MatchingMethod::Cosine:
        return 1.0 - math::Math::get_cosine_similarity(
            leftDescriptor,
            rightDescriptor);
    case MatchingMethod::Manhattan:
        return math::Math::get_manhattan_distance(
            leftDescriptor,
            rightDescriptor);
    case MatchingMethod::Euclidean:
    default:
        return math::Math::get_euclidean_distance(
            leftDescriptor,
            rightDescriptor);
    }
}

const std::vector<std::string>& descriptorJointNames()
{
    static const std::vector<std::string> names = {
        "thumb_metacarpal", "thumb_proximal", "thumb_distal", "thumb_tip",
        "index_metacarpal", "index_proximal", "index_middle", "index_distal", "index_tip",
        "middle_metacarpal", "middle_proximal", "middle_middle", "middle_distal", "middle_tip",
        "ring_metacarpal", "ring_proximal", "ring_middle", "ring_distal", "ring_tip",
        "pinky_metacarpal", "pinky_proximal", "pinky_middle", "pinky_distal", "pinky_tip"
    };
    return names;
}

json descriptorBoneMapping()
{
    return {
        {"thumb", {
            {"metacarpal", "thumb_metacarpal"},
            {"proximal", "thumb_proximal"},
            {"distal", "thumb_distal"},
            {"tip", "thumb_tip"}}},
        {"index", {
            {"metacarpal", "index_metacarpal"},
            {"proximal", "index_proximal"},
            {"middle", "index_middle"},
            {"distal", "index_distal"},
            {"tip", "index_tip"}}},
        {"middle", {
            {"metacarpal", "middle_metacarpal"},
            {"proximal", "middle_proximal"},
            {"middle", "middle_middle"},
            {"distal", "middle_distal"},
            {"tip", "middle_tip"}}},
        {"ring", {
            {"metacarpal", "ring_metacarpal"},
            {"proximal", "ring_proximal"},
            {"middle", "ring_middle"},
            {"distal", "ring_distal"},
            {"tip", "ring_tip"}}},
        {"pinky", {
            {"metacarpal", "pinky_metacarpal"},
            {"proximal", "pinky_proximal"},
            {"middle", "pinky_middle"},
            {"distal", "pinky_distal"},
            {"tip", "pinky_tip"}}}
    };
}

std::vector<JointLocationTrack> toUpstreamLocationTracks(
    const handshapeSegmentationBridge::HandSkeletonTrack& source)
{
    const std::vector<std::string>& names = descriptorJointNames();
    std::vector<JointLocationTrack> tracks(names.size());
    for (size_t jointIndex = 0; jointIndex < names.size(); ++jointIndex)
    {
        tracks[jointIndex].name = names[jointIndex];
        tracks[jointIndex].locations.reserve(source.frames.size());
        for (const std::vector<handshapeSegmentationBridge::Position>& frame : source.frames)
        {
            const handshapeSegmentationBridge::Position& position = frame[jointIndex];
            tracks[jointIndex].locations.emplace_back(position.x, position.y, position.z);
        }
    }
    return tracks;
}
}

void handshapeSegmentationBridge::prepareQuaternionTrack(
    std::vector<handshapeSegmentationBridge::Quaternion>& rotations,
    const double smoothingSigma)
{
    std::vector<Eigen::Quaterniond> upstreamRotations =
        toUpstreamRotations(rotations);
    math::Math::align_quaternion_signs(upstreamRotations);
    math::Math::smooth_quaternions(upstreamRotations, smoothingSigma);
    for (size_t index = 0; index < rotations.size(); ++index)
    {
        rotations[index] = {
            upstreamRotations[index].w(),
            upstreamRotations[index].x(),
            upstreamRotations[index].y(),
            upstreamRotations[index].z()
        };
    }
}

std::vector<double> handshapeSegmentationBridge::aggregateQuaternionAcceleration(
    const std::vector<std::vector<handshapeSegmentationBridge::Quaternion>>& quaternionTracks,
    const double deltaTime)
{
    std::vector<std::vector<Eigen::Quaterniond>> upstreamTracks;
    upstreamTracks.reserve(quaternionTracks.size());
    for (const std::vector<handshapeSegmentationBridge::Quaternion>& track : quaternionTracks)
    {
        upstreamTracks.push_back(toUpstreamRotations(track));
    }
    return math::Math::aggregate_quaternion_data(upstreamTracks, deltaTime);
}

void handshapeSegmentationBridge::smoothValues(
    std::vector<double>& values,
    const double smoothingSigma)
{
    math::Math::smooth_values(values, smoothingSigma);
}

std::vector<handshapeSegmentationBridge::Segment>
handshapeSegmentationBridge::segmentByAcceleration(
    const std::vector<double>& acceleration,
    const double thresholdFactor,
    const int minimumLength,
    const double minimumThreshold)
{
    const std::vector<::Segment> upstreamSegments =
        math::Math::segment_handshapes_by_acceleration(
            acceleration,
            thresholdFactor,
            minimumLength,
            minimumThreshold);
    std::vector<handshapeSegmentationBridge::Segment> segments;
    segments.reserve(upstreamSegments.size());
    for (const ::Segment& segment : upstreamSegments)
    {
        segments.push_back({segment.start, segment.end});
    }
    return segments;
}

std::vector<handshapeSegmentationBridge::Match>
handshapeSegmentationBridge::matchPoses(
    const std::vector<handshapeSegmentationBridge::Pose>& handshapes,
    const handshapeSegmentationBridge::Pose& inputPose,
    const handshapeSegmentationBridge::MatchingMethod method)
{
    std::vector<::Pose> upstreamHandshapes;
    upstreamHandshapes.reserve(handshapes.size());
    for (const handshapeSegmentationBridge::Pose& handshape : handshapes)
    {
        upstreamHandshapes.push_back(toUpstreamPose(handshape));
    }

    const ::Pose upstreamInputPose = toUpstreamPose(inputPose);
    const ::MatchingMethod upstreamMethod = toUpstreamMatchingMethod(method);
    std::vector<::Match> upstreamMatches;
    upstreamMatches.reserve(upstreamHandshapes.size());
    for (size_t handshapeIndex = 0;
         handshapeIndex < upstreamHandshapes.size();
         ++handshapeIndex)
    {
        upstreamMatches.push_back({
            static_cast<int>(handshapeIndex),
            poseDistance(
                upstreamHandshapes[handshapeIndex],
                upstreamInputPose,
                upstreamMethod)});
    }
    std::sort(
        upstreamMatches.begin(),
        upstreamMatches.end(),
        [](const ::Match& left, const ::Match& right)
        {
            return left.distance < right.distance;
        });

    std::vector<handshapeSegmentationBridge::Match> matches;
    matches.reserve(upstreamMatches.size());
    for (const ::Match& match : upstreamMatches)
    {
        matches.push_back({match.handshape_id, match.distance});
    }
    return matches;
}

std::vector<handshapeSegmentationBridge::Match>
handshapeSegmentationBridge::matchHandSkeletonTracks(
    const std::vector<handshapeSegmentationBridge::HandSkeletonTrack>& handshapes,
    const handshapeSegmentationBridge::HandSkeletonTrack& inputTrack,
    const int inputStartFrame,
    const int inputEndFrame,
    const handshapeSegmentationBridge::MatchingMethod method,
    handshapeSegmentationBridge::HandSkeletonMatchDiagnostics* diagnostics)
{
    const json boneMapping = descriptorBoneMapping();
    std::vector<::HandSkeleton> upstreamHandshapes;
    upstreamHandshapes.reserve(handshapes.size());
    for (const handshapeSegmentationBridge::HandSkeletonTrack& handshape : handshapes)
    {
        const std::vector<JointLocationTrack> tracks =
            toUpstreamLocationTracks(handshape);
        upstreamHandshapes.push_back(handskel::Handskel::get_average_handskeleton_from_segment(
            tracks,
            {0, static_cast<int>(handshape.frames.size()) - 1},
            boneMapping,
            false));
    }

    const std::vector<JointLocationTrack> inputTracks =
        toUpstreamLocationTracks(inputTrack);
    const ::HandSkeleton inputSkeleton = handskel::Handskel::get_average_handskeleton_from_segment(
        inputTracks,
        {inputStartFrame, inputEndFrame},
        boneMapping,
        true);
    if (diagnostics != nullptr)
    {
        diagnostics->inputDescriptor =
            handshape::Handshape::compute_handshape_representation(inputSkeleton);
        diagnostics->handshapeDescriptors.clear();
        diagnostics->handshapeDescriptors.reserve(upstreamHandshapes.size());
        for (const ::HandSkeleton& handshape : upstreamHandshapes)
        {
            diagnostics->handshapeDescriptors.push_back(
                handshape::Handshape::compute_handshape_representation(handshape));
        }
    }
    const std::vector<::Match> upstreamMatches = handskel::Handskel::match_handskeletons(
        upstreamHandshapes,
        inputSkeleton,
        toUpstreamMatchingMethod(method));

    std::vector<handshapeSegmentationBridge::Match> matches;
    matches.reserve(upstreamMatches.size());
    for (const ::Match& match : upstreamMatches)
    {
        matches.push_back({match.handshape_id, match.distance});
    }
    return matches;
}
