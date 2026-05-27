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

    bool samplePhoton(Vector3f& origin,
                      Vector3f& direction,
                      Vector3f& power,
                      int totalForThisLight) const override
    {
        if (!enabled_ || totalForThisLight <= 0) return false;

        // Uniform point on the sphere surface; its outward normal is also
        // the local emission hemisphere axis.
        float u1 = get_random_float();
        float u2 = get_random_float();
        float z = 1.0f - 2.0f * u1;
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float phi = 2.0f * static_cast<float>(M_PI) * u2;
        Vector3f n(std::cos(phi) * r, z, std::sin(phi) * r);
        origin = position_ + n * radius_;

        // Cosine-weighted hemisphere around `n`.
        float v1 = get_random_float();
        float v2 = get_random_float();
        float rr = std::sqrt(v1);
        float phi2 = 2.0f * static_cast<float>(M_PI) * v2;
        float lx = rr * std::cos(phi2);
        float ly = rr * std::sin(phi2);
        float lz = std::sqrt(std::max(0.0f, 1.0f - v1));
        // Build a local frame around n.
        Vector3f tan = std::fabs(n.y) > 0.999f
                           ? Vector3f(1.0f, 0.0f, 0.0f)
                           : normalize(crossProduct(Vector3f(0.0f, 1.0f, 0.0f), n));
        Vector3f bit = normalize(crossProduct(n, tan));
        direction = normalize(tan * lx + bit * ly + n * lz);

        const Vector3f Phi = emittedRadiance() * (static_cast<float>(M_PI) * area_);
        power = Phi / static_cast<float>(totalForThisLight);
        return true;
    }

    bool sampleEmissionPoint(Vector3f& origin,
                             Vector3f& normal,
                             Vector3f& emissionRadiance,
                             float& area) const override
    {
        if (!enabled_) return false;
        float u1 = get_random_float();
        float u2 = get_random_float();
        float z = 1.0f - 2.0f * u1;
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float phi = 2.0f * static_cast<float>(M_PI) * u2;
        normal = Vector3f(std::cos(phi) * r, z, std::sin(phi) * r);
        origin = position_ + normal * radius_;
        emissionRadiance = emittedRadiance();
        area = area_;
        return true;
    }

private:
    Vector3f position_ = Vector3f(0.0f);
    float    radius_   = 0.5f;
    float    area_     = 1.0f;
};

#endif // PROJECT2_AREA_SPHERE_LIGHT_H
