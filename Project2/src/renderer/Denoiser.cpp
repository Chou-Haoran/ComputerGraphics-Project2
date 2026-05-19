#include "Denoiser.hpp"

#include "global.hpp"

#include <algorithm>
#include <cmath>

namespace Denoiser {
namespace {

float gaussianWeight(float distance2, float sigma)
{
    float denom = 2.0f * sigma * sigma;
    return std::exp(-distance2 / std::max(denom, 1e-8f));
}

float squaredLength(const Vector3f& v)
{
    return dotProduct(v, v);
}

} // namespace

std::vector<Vector3f> jointBilateralFilter(const std::vector<Vector3f>& color,
                                           const GuideBuffers& guides,
                                           int width, int height,
                                           const Settings& settings)
{
    if (settings.radius <= 0 || width <= 0 || height <= 0 ||
        static_cast<int>(color.size()) != width * height) {
        return color;
    }

    std::vector<Vector3f> out(color.size(), Vector3f(0.0f));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int centerIdx = x + y * width;
            const Vector3f centerColor = color[centerIdx];
            const Vector3f centerAlbedo = guides.albedo[centerIdx];
            const Vector3f centerNormal = guides.normal[centerIdx];
            const bool centerValid = guides.valid[centerIdx] != 0;

            Vector3f accum(0.0f);
            float weightSum = 0.0f;

            for (int dy = -settings.radius; dy <= settings.radius; ++dy) {
                int sy = std::clamp(y + dy, 0, height - 1);
                for (int dx = -settings.radius; dx <= settings.radius; ++dx) {
                    int sx = std::clamp(x + dx, 0, width - 1);
                    int sampleIdx = sx + sy * width;

                    bool sampleValid = guides.valid[sampleIdx] != 0;
                    if (centerValid != sampleValid) continue;

                    float spatialDist2 = static_cast<float>(dx * dx + dy * dy);
                    float weight = gaussianWeight(spatialDist2, settings.sigmaSpatial);

                    Vector3f colorDiff = color[sampleIdx] - centerColor;
                    weight *= gaussianWeight(squaredLength(colorDiff), settings.sigmaColor);

                    if (centerValid && sampleValid) {
                        float normalCos = clamp(-1.0f, 1.0f,
                                                dotProduct(centerNormal, guides.normal[sampleIdx]));
                        float normalDist = 1.0f - std::max(0.0f, normalCos);
                        weight *= gaussianWeight(normalDist * normalDist, settings.sigmaNormal);

                        Vector3f albedoDiff = guides.albedo[sampleIdx] - centerAlbedo;
                        weight *= gaussianWeight(squaredLength(albedoDiff), settings.sigmaAlbedo);
                    }

                    accum += color[sampleIdx] * weight;
                    weightSum += weight;
                }
            }

            out[centerIdx] = weightSum > 1e-8f ? accum / weightSum : centerColor;
        }
    }

    return out;
}

} // namespace Denoiser
