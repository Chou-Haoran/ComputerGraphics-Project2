#pragma once

#ifndef PROJECT2_AREA_RECT_LIGHT_H
#define PROJECT2_AREA_RECT_LIGHT_H

#include "Light.hpp"
#include "global.hpp"

class AreaRectLight final : public Light
{
public:
    AreaRectLight(const Vector3f& position,
                  const Vector3f& normal,
                  const Vector3f& size,
                  const Vector3f& emission,
                  float intensity,
                  bool enabled = true)
        : Light(LightType::AreaRect, emission, intensity, enabled)
        , position_(position)
        , size_(size)
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
        area_ = std::max(size_.x * size_.y, EPSILON);
    }

    bool sampleLi(const Vector3f& shadingPoint, LightSample& sample) const override
    {
        if (!enabled_) return false;

        const float sx = size_.x * (get_random_float() - 0.5f);
        const float sz = size_.y * (get_random_float() - 0.5f);
        const Vector3f lightPoint = position_ + tangent_ * sx + bitangent_ * sz;

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

    bool samplePhoton(Vector3f& origin,
                      Vector3f& direction,
                      Vector3f& power,
                      int totalForThisLight) const override
    {
        if (!enabled_ || totalForThisLight <= 0) return false;

        const float sx = size_.x * (get_random_float() - 0.5f);
        const float sz = size_.y * (get_random_float() - 0.5f);
        origin = position_ + tangent_ * sx + bitangent_ * sz;

        // Cosine-weighted hemisphere around the light's outward normal.
        // (Lambertian emitter — matches our radiance-based `sampleLi`.)
        float u1 = get_random_float();
        float u2 = get_random_float();
        float r  = std::sqrt(u1);
        float phi = 2.0f * static_cast<float>(M_PI) * u2;
        float lx = r * std::cos(phi);
        float ly = r * std::sin(phi);
        float lz = std::sqrt(std::max(0.0f, 1.0f - u1));
        direction = normalize(tangent_ * lx + bitangent_ * ly + normal_ * lz);

        // Total radiant flux of a Lambertian rect is Φ = π · L · A; with
        // cosine-distributed photons the per-sample power is Φ / N.
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
        const float sx = size_.x * (get_random_float() - 0.5f);
        const float sz = size_.y * (get_random_float() - 0.5f);
        origin = position_ + tangent_ * sx + bitangent_ * sz;
        normal = normal_;
        emissionRadiance = emittedRadiance();
        area = area_;
        return true;
    }

private:
    Vector3f position_  = Vector3f(0.0f);
    Vector3f normal_    = Vector3f(0.0f, -1.0f, 0.0f);
    Vector3f tangent_   = Vector3f(1.0f, 0.0f, 0.0f);
    Vector3f bitangent_ = Vector3f(0.0f, 0.0f, 1.0f);
    Vector3f size_      = Vector3f(1.0f, 1.0f, 0.0f);
    float    area_      = 1.0f;
};

#endif // PROJECT2_AREA_RECT_LIGHT_H
