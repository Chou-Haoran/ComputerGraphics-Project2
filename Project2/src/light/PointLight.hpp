#pragma once

#ifndef PROJECT2_POINT_LIGHT_H
#define PROJECT2_POINT_LIGHT_H

#include "Light.hpp"
#include "global.hpp"

class PointLight final : public Light
{
public:
    PointLight(const Vector3f& position,
               const Vector3f& emission,
               float intensity,
               float range = 0.0f,
               bool enabled = true)
        : Light(LightType::Point, emission, intensity, enabled)
        , position_(position)
        , range_(range)
    {
    }

    bool sampleLi(const Vector3f& shadingPoint, LightSample& sample) const override
    {
        if (!enabled_) return false;

        Vector3f toLight = position_ - shadingPoint;
        float dist2 = dotProduct(toLight, toLight);
        if (dist2 <= EPSILON) return false;

        float dist = std::sqrt(dist2);
        if (range_ > 0.0f && dist > range_) return false;

        sample.direction = toLight / dist;
        sample.radiance = emittedRadiance() / dist2;
        sample.pdf = 1.0f;
        sample.maxDistance = dist;
        sample.infiniteDistance = false;
        sample.delta = true;
        return true;
    }

    float samplingWeight() const override
    {
        return std::max(4.0f * static_cast<float>(M_PI) * luminance(emittedRadiance()), 1e-4f);
    }

private:
    Vector3f position_ = Vector3f(0.0f);
    float    range_    = 0.0f;
};

#endif // PROJECT2_POINT_LIGHT_H
