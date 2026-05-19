#ifndef PROJECT2_AREA_LIGHT_H
#define PROJECT2_AREA_LIGHT_H

#include "Vector.hpp"

// In the current Monte Carlo pipeline an area light is just a MeshTriangle
// (or Sphere) whose Material is `EMIT` with non-zero m_emission. The Scene
// discovers them via Object::hasEmit() and importance-samples them in
// sampleLight(). This header keeps a logical "AreaLight" descriptor used
// by SceneLoader / configs so that PDF requirement (v)
// "light source position, size, and number" can be authored cleanly.
//
// TODO: extend with shape kind (rect / disk / sphere), color temperature,
// IES profile path, and a `enabled` toggle for shadow on/off experiments.
struct AreaLight {
    Vector3f position   = {0, 0, 0};   // world-space center
    Vector3f normal     = {0, -1, 0};  // surface normal (rect lights)
    Vector3f size       = {1, 1, 0};   // half-extents along local u, v
    Vector3f emission   = {1, 1, 1};   // radiance (W·sr⁻¹·m⁻²)
    float    intensity  = 1.0f;        // scalar multiplier
    bool     enabled    = true;
    // TODO: SceneLoader emits a MeshTriangle quad from these fields and
    // attaches an EMIT material with m_emission = emission * intensity.
};

#endif // PROJECT2_AREA_LIGHT_H
