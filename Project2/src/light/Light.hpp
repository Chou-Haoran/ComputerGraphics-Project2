#pragma once

#ifndef PROJECT2_LIGHT_H
#define PROJECT2_LIGHT_H

#include "LightType.hpp"
#include "Vector.hpp"

#include <limits>

struct LightSample {
    Vector3f direction = Vector3f(0.0f);  // shading point -> light
    Vector3f radiance  = Vector3f(0.0f);  // incident radiance along direction
    float pdf          = 1.0f;            // directional PDF (solid angle)
    float maxDistance  = std::numeric_limits<float>::infinity();
    bool infiniteDistance = false;
    bool delta = false;
};

class Light {
public:
    Light(LightType type, const Vector3f& emission, float intensity, bool enabled)
        : type_(type), emission_(emission), intensity_(intensity), enabled_(enabled)
    {
    }

    virtual ~Light() = default;

    LightType type() const { return type_; }
    bool enabled() const { return enabled_; }

    virtual bool sampleLi(const Vector3f& shadingPoint, LightSample& sample) const = 0;
    virtual float samplingWeight() const
    {
        return std::max(luminance(emittedRadiance()), 1e-4f);
    }

protected:
    Vector3f emittedRadiance() const { return emission_ * intensity_; }
    float luminance(const Vector3f& c) const
    {
        return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
    }

    LightType type_;
    Vector3f  emission_;
    float     intensity_ = 1.0f;
    bool      enabled_   = true;
};

#endif // PROJECT2_LIGHT_H
