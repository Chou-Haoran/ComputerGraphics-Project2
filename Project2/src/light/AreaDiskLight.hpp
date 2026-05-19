#pragma once

#ifndef PROJECT2_AREA_DISK_LIGHT_H
#define PROJECT2_AREA_DISK_LIGHT_H

#include "Light.hpp"
#include "global.hpp"

class AreaDiskLight final : public Light
{
public:
    AreaDiskLight(const Vector3f& position,
                  const Vector3f& normal,
                  float radius,
                  const Vector3f& emission,
                  float intensity,
                  bool enabled = true)
        : Light(LightType::AreaDisk, emission, intensity, enabled)
        , position_(position)
        , radius_(std::max(radius, EPSILON))
        , area_(std::max(static_cast<float>(M_PI) * radius_ * radius_, EPSILON))
    {
        normal_ = normalize(normal);
        if (dotProduct(normal_, normal_) <= EPSILON) {
            normal_ = Vector3f(0.0f, -1.0f, 0.0f);
        }

        if (std::fabs(normal_.y) > 0.999f) {
            tangent_ = Vector3f(1.0f, 0.0f, 0.0f);
        } else {
            tangent_ = normalize(crossProduct(Vector3f(0.0f, 1.0f, 0.0f), normal_));
        }
        bitangent_ = normalize(crossProduct(normal_, tangent_));
    }

    bool sampleLi(const Vector3f& shadingPoint, LightSample& sample) const override
    {
        if (!enabled_) return false;

        float r = radius_ * std::sqrt(get_random_float());
        float phi = 2.0f * static_cast<float>(M_PI) * get_random_float();
        Vector3f lightPoint = position_ +
                              tangent_ * (std::cos(phi) * r) +
                              bitangent_ * (std::sin(phi) * r);

        Vector3f toLight = lightPoint - shadingPoint;
        float dist2 = dotProduct(toLight, toLight);
        if (dist2 <= EPSILON) return false;

        float dist = std::sqrt(dist2);
        Vector3f wi = toLight / dist;
        float cosThetaLight = std::max(0.0f, dotProduct(normal_, -wi));
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
    Vector3f position_  = Vector3f(0.0f);
    Vector3f normal_    = Vector3f(0.0f, -1.0f, 0.0f);
    Vector3f tangent_   = Vector3f(1.0f, 0.0f, 0.0f);
    Vector3f bitangent_ = Vector3f(0.0f, 0.0f, 1.0f);
    float    radius_    = 0.5f;
    float    area_      = 1.0f;
};

#endif // PROJECT2_AREA_DISK_LIGHT_H
