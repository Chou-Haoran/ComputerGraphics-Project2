#ifndef PROJECT2_ENVMAP_H
#define PROJECT2_ENVMAP_H

#include "Vector.hpp"
#include <string>
#include <vector>

// HDR environment map (image-based lighting). Powers PDF requirement (vi)
// "environment map selection". When valid(), Scene::castRay's miss branch
// returns sample(dir) instead of backgroundColor.
class EnvironmentMap {
public:
    EnvironmentMap() = default;
    ~EnvironmentMap();

    // TODO: load equirectangular .hdr via stb_image (with HDR support).
    bool loadHDR(const std::string& path);

    // dir must be a unit vector. Returns background when no map is loaded.
    Vector3f sample(const Vector3f& dir) const;
    bool sampleDirection(Vector3f& dir, Vector3f& radiance, float& pdf) const;
    float pdf(const Vector3f& dir) const;

    bool valid() const { return data != nullptr; }

    // Strength multiplier to mix env intensity with backgroundColor.
    float intensity = 1.0f;
    float rotationYDegrees = 0.0f;

    // TODO: importance sampling of envmap luminance (alias method or
    // 2D inverse-CDF) — yields much lower variance on glossy/glass hits.

private:
    void buildImportanceDistribution();
    void directionToUV(const Vector3f& dir, float& u, float& v) const;
    Vector3f sampleTexel(int x, int y) const;
    float* data = nullptr;
    int    w = 0;
    int    h = 0;
    std::vector<float> marginalCdf;
    std::vector<float> conditionalCdf;
    std::vector<float> texelWeights;
    float totalWeight = 0.0f;
};

#endif // PROJECT2_ENVMAP_H
