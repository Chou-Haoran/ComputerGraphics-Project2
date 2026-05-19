#pragma once

#ifndef PROJECT2_LIGHT_SPEC_H
#define PROJECT2_LIGHT_SPEC_H

#include "LightType.hpp"
#include "Vector.hpp"

struct LightSpec {
    LightType type = LightType::AreaRect;
    Vector3f position = {0, 0, 0};
    Vector3f direction = {0, -1, 0};
    Vector3f size = {1, 1, 0};
    float radius = 0.5f;
    Vector3f emission = {1, 1, 1};
    float intensity = 1.0f;
    float range = 0.0f;
    float innerAngleDeg = 15.0f;
    float outerAngleDeg = 25.0f;
    bool enabled = true;
    bool showModel = false;
};

#endif // PROJECT2_LIGHT_SPEC_H
