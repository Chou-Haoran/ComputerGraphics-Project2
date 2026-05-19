#pragma once

#ifndef PROJECT2_DIRECTIONAL_LIGHT_H
#define PROJECT2_DIRECTIONAL_LIGHT_H

#include "Light.hpp"
#include "global.hpp"

class DirectionalLight final : public Light
{
public:
    DirectionalLight(const Vector3f& direction,
                     const Vector3f& emission,
                     float intensity,
                     bool enabled = true)
        : Light(LightType::Directional, emission, intensity, enabled)
        , direction_(normalize(direction))
    {
    }

    bool sampleLi(const Vector3f&, LightSample& sample) const override
    {
        if (!enabled_) return false;
        if (dotProduct(direction_, direction_) <= EPSILON) return false;

        sample.direction = -direction_;
        sample.radiance = emittedRadiance();
        sample.pdf = 1.0f;
        sample.maxDistance = std::numeric_limits<float>::infinity();
        sample.infiniteDistance = true;
        sample.delta = true;
        return true;
    }

    float samplingWeight() const override
    {
        return std::max(luminance(emittedRadiance()), 1e-4f);
    }

private:
    Vector3f direction_ = Vector3f(0.0f, -1.0f, 0.0f);
};

#endif // PROJECT2_DIRECTIONAL_LIGHT_H
