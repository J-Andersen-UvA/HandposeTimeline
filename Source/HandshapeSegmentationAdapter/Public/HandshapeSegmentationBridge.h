#pragma once

#include <vector>

namespace handshapeSegmentationBridge
{
enum class MatchingMethod
{
    Euclidean,
    Cosine,
    Manhattan
};

struct Position
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct HandSkeletonTrack
{
    std::vector<std::vector<Position>> frames;
};

struct Quaternion
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Pose
{
    std::vector<Quaternion> joints;
};

struct Match
{
    int handshapeIndex = -1;
    double distance = 0.0;
};

struct HandSkeletonMatchDiagnostics
{
    std::vector<double> inputDescriptor;
    std::vector<std::vector<double>> handshapeDescriptors;
};

struct Segment
{
    int start = 0;
    int end = 0;
};

void prepareQuaternionTrack(
    std::vector<Quaternion>& rotations,
    double smoothingSigma);
std::vector<double> aggregateQuaternionAcceleration(
    const std::vector<std::vector<Quaternion>>& quaternionTracks,
    double deltaTime);
void smoothValues(std::vector<double>& values, double smoothingSigma);
std::vector<Segment> segmentByAcceleration(
    const std::vector<double>& acceleration,
    double thresholdFactor,
    int minimumLength,
    double minimumThreshold);

std::vector<Match> matchPoses(
    const std::vector<Pose>& handshapes,
    const Pose& inputPose,
    MatchingMethod method);
std::vector<Match> matchHandSkeletonTracks(
    const std::vector<HandSkeletonTrack>& handshapes,
    const HandSkeletonTrack& inputTrack,
    int inputStartFrame,
    int inputEndFrame,
    MatchingMethod method,
    HandSkeletonMatchDiagnostics* diagnostics);
}
