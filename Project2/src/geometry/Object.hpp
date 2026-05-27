//
// Created by LEI XU on 5/13/19.
//
#pragma once
#ifndef RAYTRACING_OBJECT_H
#define RAYTRACING_OBJECT_H

#include "Vector.hpp"
#include "global.hpp"
#include "Bounds3.hpp"
#include "Ray.hpp"
#include "Intersection.hpp"

class Material;

class Object
{
public:
    Object() {}
    virtual ~Object() {}
    virtual Intersection getIntersection(Ray _ray) = 0;
    virtual void getSurfaceProperties(const Vector3f &, const Vector3f &, const uint32_t &, const Vector2f &, Vector3f &, Vector2f &) const = 0;
    virtual Vector3f evalDiffuseColor(const Vector2f &) const =0;
    virtual Bounds3 getBounds()=0;
    virtual float getArea()=0;
    virtual void Sample(Intersection &pos, float &pdf)=0;
    virtual bool hasEmit()=0;
    // Defaults to nullptr so non-shaded helpers don't need to implement it;
    // mesh objects override to expose the material the photon-emission pass
    // uses for importance-sampling toward specular targets.
    virtual const Material* getMaterial() const { return nullptr; }
};



#endif //RAYTRACING_OBJECT_H
