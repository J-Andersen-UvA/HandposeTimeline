#include "HandshapeSegmentationBridge.h"

#include "matching/poseshape.h"
#include "math/math.h"

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

    const std::vector<::Match> upstreamMatches =
        poseshape::Poseshape::match_poses(
            upstreamHandshapes,
            toUpstreamPose(inputPose),
            toUpstreamMatchingMethod(method));

    std::vector<handshapeSegmentationBridge::Match> matches;
    matches.reserve(upstreamMatches.size());
    for (const ::Match& match : upstreamMatches)
    {
        matches.push_back({match.handshape_id, match.distance});
    }
    return matches;
}
