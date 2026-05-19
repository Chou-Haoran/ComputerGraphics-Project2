#pragma once

#include "Vector.hpp"

#include <cstdint>
#include <vector>

namespace Denoiser {

struct GuideBuffers {
    std::vector<Vector3f> albedo;
    std::vector<Vector3f> normal;
    std::vector<uint8_t>  valid;
};

struct Settings {
    int   radius = 3;
    float sigmaSpatial = 2.0f;
    float sigmaColor = 0.15f;
    float sigmaNormal = 0.2f;
    float sigmaAlbedo = 0.25f;
};

std::vector<Vector3f> jointBilateralFilter(const std::vector<Vector3f>& color,
                                           const GuideBuffers& guides,
                                           int width, int height,
                                           const Settings& settings);

} // namespace Denoiser
