#include "HandshapeSegmentationMath.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace handshapeSegmentation
{
namespace
{
double radialAngleBetweenQuaternions(
    const Eigen::Quaterniond& first,
    const Eigen::Quaterniond& second)
{
    const double dot = first.normalized().dot(second.normalized());
    return 2.0 * std::acos(std::abs(std::clamp(dot, -1.0, 1.0)));
}

double angularAccelerationAtFrame(
    const std::vector<Eigen::Quaterniond>& quaternions,
    int frameIndex,
    double deltaTime)
{
    const double previousSpeed = radialAngleBetweenQuaternions(
        quaternions[frameIndex - 1],
        quaternions[frameIndex]) / deltaTime;
    const double nextSpeed = radialAngleBetweenQuaternions(
        quaternions[frameIndex],
        quaternions[frameIndex + 1]) / deltaTime;
    return (nextSpeed - previousSpeed) / deltaTime;
}

std::vector<double> makeGaussianKernel(double sigma)
{
    const int radius = static_cast<int>(std::ceil(3.0 * sigma));
    std::vector<double> kernel(2 * radius + 1);
    double sum = 0.0;
    for (int offset = -radius; offset <= radius; ++offset)
    {
        const double value = std::exp(
            -0.5 * static_cast<double>(offset * offset) / (sigma * sigma));
        kernel[offset + radius] = value;
        sum += value;
    }
    for (double& value : kernel)
    {
        value /= sum;
    }
    return kernel;
}
}

void alignQuaternionSigns(std::vector<Eigen::Quaterniond>& quaternions)
{
    for (size_t index = 1; index < quaternions.size(); ++index)
    {
        if (quaternions[index - 1].dot(quaternions[index]) < 0.0)
        {
            quaternions[index].coeffs() *= -1.0;
        }
    }
}

void smoothQuaternions(
    std::vector<Eigen::Quaterniond>& quaternions,
    double sigma)
{
    if (quaternions.empty())
    {
        return;
    }

    const std::vector<double> kernel = makeGaussianKernel(sigma);
    const int radius = static_cast<int>((kernel.size() - 1) / 2);
    std::vector<Eigen::Quaterniond> smoothed = quaternions;

    for (int frameIndex = 0;
         frameIndex < static_cast<int>(quaternions.size());
         ++frameIndex)
    {
        Eigen::Quaterniond value(0.0, 0.0, 0.0, 0.0);
        double weightSum = 0.0;
        for (int offset = -radius; offset <= radius; ++offset)
        {
            const int sourceIndex = frameIndex + offset;
            if (sourceIndex < 0 ||
                sourceIndex >= static_cast<int>(quaternions.size()))
            {
                continue;
            }
            const double weight = kernel[offset + radius];
            value.coeffs() += weight * quaternions[sourceIndex].coeffs();
            weightSum += weight;
        }
        value.coeffs() /= weightSum;
        value.normalize();
        smoothed[frameIndex] = value;
    }
    quaternions = std::move(smoothed);
}

void smoothValues(std::vector<double>& values, double sigma)
{
    if (values.empty())
    {
        return;
    }

    const std::vector<double> kernel = makeGaussianKernel(sigma);
    const int radius = static_cast<int>((kernel.size() - 1) / 2);
    std::vector<double> smoothed(values.size(), 0.0);

    for (int frameIndex = 0;
         frameIndex < static_cast<int>(values.size());
         ++frameIndex)
    {
        double weightSum = 0.0;
        for (int offset = -radius; offset <= radius; ++offset)
        {
            const int sourceIndex = frameIndex + offset;
            if (sourceIndex < 0 || sourceIndex >= static_cast<int>(values.size()))
            {
                continue;
            }
            const double weight = kernel[offset + radius];
            smoothed[frameIndex] += weight * values[sourceIndex];
            weightSum += weight;
        }
        smoothed[frameIndex] /= weightSum;
    }
    values = std::move(smoothed);
}

std::vector<double> aggregateQuaternionAcceleration(
    const std::vector<std::vector<Eigen::Quaterniond>>& quaternionTracks,
    double deltaTime)
{
    if (quaternionTracks.empty() || quaternionTracks.front().size() < 3)
    {
        return {};
    }

    const int frameCount = static_cast<int>(quaternionTracks.front().size());
    std::vector<double> acceleration(frameCount, 0.0);
    for (int frameIndex = 1; frameIndex < frameCount - 1; ++frameIndex)
    {
        double sum = 0.0;
        for (const std::vector<Eigen::Quaterniond>& track : quaternionTracks)
        {
            sum += angularAccelerationAtFrame(track, frameIndex, deltaTime);
        }
        acceleration[frameIndex] = sum / quaternionTracks.size();
    }
    acceleration.front() = acceleration[1];
    acceleration.back() = acceleration[frameCount - 2];
    return acceleration;
}

std::vector<Segment> segmentByAcceleration(
    const std::vector<double>& acceleration,
    double thresholdFactor,
    int minimumLength,
    double minimumThreshold)
{
    std::vector<Segment> segments;
    const int frameCount = static_cast<int>(acceleration.size());
    if (frameCount < 3)
    {
        return segments;
    }

    const double mean = std::accumulate(
        acceleration.begin(),
        acceleration.end(),
        0.0) / frameCount;
    double variance = 0.0;
    for (const double value : acceleration)
    {
        variance += (value - mean) * (value - mean);
    }
    const double threshold = std::max(
        thresholdFactor * std::sqrt(variance / frameCount),
        minimumThreshold);

    struct Extremum
    {
        int index;
        bool isPeak;
    };
    std::vector<Extremum> found;
    for (int frameIndex = 1; frameIndex < frameCount - 1; ++frameIndex)
    {
        if (acceleration[frameIndex] > threshold &&
            acceleration[frameIndex] > acceleration[frameIndex - 1] &&
            acceleration[frameIndex] >= acceleration[frameIndex + 1])
        {
            found.push_back({frameIndex, true});
        }
        else if (acceleration[frameIndex] < -threshold &&
                 acceleration[frameIndex] < acceleration[frameIndex - 1] &&
                 acceleration[frameIndex] <= acceleration[frameIndex + 1])
        {
            found.push_back({frameIndex, false});
        }
    }

    std::vector<Extremum> extrema;
    for (const Extremum& current : found)
    {
        if (!extrema.empty() && extrema.back().isPeak == current.isPeak)
        {
            const bool currentIsStronger = current.isPeak
                ? acceleration[current.index] > acceleration[extrema.back().index]
                : acceleration[current.index] < acceleration[extrema.back().index];
            if (currentIsStronger)
            {
                extrema.back() = current;
            }
        }
        else
        {
            extrema.push_back(current);
        }
    }

    if (extrema.empty())
    {
        return {{0, frameCount - 1}};
    }

    const auto addSegment = [&](int start, int end)
    {
        if (end - start + 1 >= minimumLength)
        {
            segments.push_back({start, end});
        }
    };

    size_t extremumIndex = 0;
    if (extrema.front().isPeak)
    {
        addSegment(0, extrema.front().index);
        extremumIndex = 1;
    }
    for (; extremumIndex < extrema.size(); extremumIndex += 2)
    {
        if (extremumIndex + 1 < extrema.size())
        {
            addSegment(
                extrema[extremumIndex].index,
                extrema[extremumIndex + 1].index);
        }
        else
        {
            addSegment(extrema[extremumIndex].index, frameCount - 1);
        }
    }
    return segments;
}
}
