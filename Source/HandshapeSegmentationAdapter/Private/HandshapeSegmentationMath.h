#pragma once

#include <Eigen/Geometry>
#include <vector>

namespace handshapeSegmentation
{
struct Segment
{
    int start = 0;
    int end = 0;
};

void alignQuaternionSigns(std::vector<Eigen::Quaterniond>& quaternions);
void smoothQuaternions(std::vector<Eigen::Quaterniond>& quaternions, double sigma);
void smoothValues(std::vector<double>& values, double sigma);
std::vector<double> aggregateQuaternionAcceleration(
    const std::vector<std::vector<Eigen::Quaterniond>>& quaternionTracks,
    double deltaTime);
std::vector<Segment> segmentByAcceleration(
    const std::vector<double>& acceleration,
    double thresholdFactor,
    int minimumLength,
    double minimumThreshold);
}
