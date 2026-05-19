#pragma once

#ifndef PROJECT2_MESH_LIGHT_H
#define PROJECT2_MESH_LIGHT_H

#include "Light.hpp"
#include "Object.hpp"
#include "global.hpp"

class MeshLight final : public Light
{
public:
    explicit MeshLight(Object* object)
        : Light(LightType::MeshEmit, Vector3f(0.0f), 1.0f, object != nullptr)
        , object_(object)
    {
    }

    bool sampleLi(const Vector3f& shadingPoint, LightSample& sample) const override
    {
        if (!enabled_ || object_ == nullptr || !object_->hasEmit()) return false;

        Intersection lightInter;
        float areaPdf = 0.0f;
        object_->Sample(lightInter, areaPdf);
        if (!lightInter.happened || lightInter.material == nullptr || areaPdf <= 0.0f) {
            return false;
        }

        Vector3f toLight = lightInter.coords - shadingPoint;
        float dist2 = dotProduct(toLight, toLight);
        if (dist2 <= EPSILON) return false;

        float dist = std::sqrt(dist2);
        Vector3f wi = toLight / dist;
        float cosThetaLight = std::max(0.0f, dotProduct(lightInter.normal, -wi));
        if (cosThetaLight <= 0.0f) return false;

        sample.direction = wi;
        sample.radiance = lightInter.material->getEmission();
        sample.pdf = areaPdf * dist2 / std::max(cosThetaLight, 1e-8f);
        sample.maxDistance = dist;
        sample.infiniteDistance = false;
        sample.delta = false;
        return true;
    }

    float samplingWeight() const override
    {
        if (!enabled_ || object_ == nullptr || !object_->hasEmit()) return 1e-4f;
        Intersection lightInter;
        float areaPdf = 0.0f;
        object_->Sample(lightInter, areaPdf);
        Vector3f emission = lightInter.material ? lightInter.material->getEmission()
                                                : Vector3f(1.0f);
        return std::max(static_cast<float>(M_PI) * object_->getArea() *
                            luminance(emission),
                        1e-4f);
    }

private:
    Object* object_ = nullptr;
};

#endif // PROJECT2_MESH_LIGHT_H
