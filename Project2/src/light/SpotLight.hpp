#pragma once

#ifndef PROJECT2_SPOT_LIGHT_H
#define PROJECT2_SPOT_LIGHT_H

#include "Light.hpp"
#include "global.hpp"

#include <algorithm>

class SpotLight final : public Light
{
public:
    SpotLight(const Vector3f& position,
              const Vector3f& direction,
              const Vector3f& emission,
              float intensity,
              float innerAngleDeg,
              float outerAngleDeg,
              float range = 0.0f,
              bool enabled = true)
        : Light(LightType::Spot, emission, intensity, enabled)
        , position_(position)
        , direction_(normalize(direction))
        , range_(range)
    {
        float inner = std::max(0.0f, std::min(innerAngleDeg, outerAngleDeg));
        float outer = std::max(inner, outerAngleDeg);
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        cosInner_ = std::cos(inner * kDegToRad);
        cosOuter_ = std::cos(outer * kDegToRad);
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
        float spotCos = dotProduct(direction_, -sample.direction);
        if (spotCos <= cosOuter_) return false;

        float spotFalloff = 1.0f;
        if (spotCos < cosInner_ && cosInner_ > cosOuter_ + EPSILON) {
            spotFalloff = (spotCos - cosOuter_) / (cosInner_ - cosOuter_);
        }
        spotFalloff = std::clamp(spotFalloff, 0.0f, 1.0f);

        sample.radiance = emittedRadiance() * spotFalloff / dist2;
        sample.pdf = 1.0f;
        sample.maxDistance = dist;
        sample.infiniteDistance = false;
        sample.delta = true;
        return spotFalloff > 0.0f;
    }

    float samplingWeight() const override
    {
        float coneScale = std::max(1.0f - cosOuter_, 0.05f);
        return std::max(2.0f * static_cast<float>(M_PI) * coneScale *
                            luminance(emittedRadiance()),
                        1e-4f);
    }

private:
    Vector3f position_  = Vector3f(0.0f);
    Vector3f direction_ = Vector3f(0.0f, -1.0f, 0.0f);
    float    range_     = 0.0f;
    float    cosInner_  = 1.0f;
    float    cosOuter_  = 1.0f;
};

#endif // PROJECT2_SPOT_LIGHT_H
