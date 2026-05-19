#pragma once

#ifndef PROJECT2_AREA_SPHERE_LIGHT_H
#define PROJECT2_AREA_SPHERE_LIGHT_H

#include "Light.hpp"
#include "global.hpp"

class AreaSphereLight final : public Light
{
public:
    AreaSphereLight(const Vector3f& position,
                    float radius,
                    const Vector3f& emission,
                    float intensity,
                    bool enabled = true)
        : Light(LightType::AreaSphere, emission, intensity, enabled)
        , position_(position)
        , radius_(std::max(radius, EPSILON))
        , area_(std::max(4.0f * static_cast<float>(M_PI) * radius_ * radius_, EPSILON))
    {
    }

    bool sampleLi(const Vector3f& shadingPoint, LightSample& sample) const override
    {
        if (!enabled_) return false;

        float u1 = get_random_float();
        float u2 = get_random_float();
        float z = 1.0f - 2.0f * u1;
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float phi = 2.0f * static_cast<float>(M_PI) * u2;
        Vector3f normal(std::cos(phi) * r, z, std::sin(phi) * r);
        Vector3f lightPoint = position_ + normal * radius_;

        Vector3f toLight = lightPoint - shadingPoint;
        float dist2 = dotProduct(toLight, toLight);
        if (dist2 <= EPSILON) return false;

        float dist = std::sqrt(dist2);
        Vector3f wi = toLight / dist;
        float cosThetaLight = std::max(0.0f, dotProduct(normal, -wi));
        if (cosThetaLight <= 0.0f) return false;

        sample.direction = wi;
        sample.radiance = emittedRadiance();
        sample.pdf = dist2 / std::max(cosThetaLight * area_, 1e-8f);
        sample.maxDistance = dist;
        sample.infiniteDistance = false;
        sample.delta = false;
        return true;
    }

    float samplingWeight() const override
    {
        return std::max(static_cast<float>(M_PI) * area_ * luminance(emittedRadiance()), 1e-4f);
    }

private:
    Vector3f position_ = Vector3f(0.0f);
    float    radius_   = 0.5f;
    float    area_     = 1.0f;
};

#endif // PROJECT2_AREA_SPHERE_LIGHT_H
