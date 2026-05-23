#include "EnvironmentMap.hpp"

#include "global.hpp"
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

float wrap01(float x)
{
    x = x - std::floor(x);
    return x < 0.0f ? x + 1.0f : x;
}

Vector3f lerpVec(const Vector3f& a, const Vector3f& b, float t)
{
    return a * (1.0f - t) + b * t;
}

Vector3f rotateY(const Vector3f& v, float degrees)
{
    float r = deg2rad(degrees);
    float c = std::cos(r);
    float s = std::sin(r);
    return Vector3f(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

float luminance(const Vector3f& c)
{
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

} // namespace

EnvironmentMap::~EnvironmentMap()
{
    if (data) {
        stbi_image_free(data);
        data = nullptr;
    }
}

bool EnvironmentMap::loadHDR(const std::string& path)
{
    if (data) {
        stbi_image_free(data);
        data = nullptr;
        w = 0;
        h = 0;
    }
    marginalCdf.clear();
    conditionalCdf.clear();
    texelWeights.clear();
    totalWeight = 0.0f;

    int channels = 0;
    data = stbi_loadf(path.c_str(), &w, &h, &channels, 3);
    if (!data) {
        std::cerr << "EnvironmentMap::loadHDR: failed to load " << path
                  << " (" << stbi_failure_reason() << ")\n";
        return false;
    }

    if (w <= 0 || h <= 0) return false;
    buildImportanceDistribution();
    return true;
}

Vector3f EnvironmentMap::sampleTexel(int x, int y) const
{
    x = ((x % w) + w) % w;
    y = std::clamp(y, 0, h - 1);
    int idx = (y * w + x) * 3;
    return Vector3f(data[idx + 0], data[idx + 1], data[idx + 2]);
}

Vector3f EnvironmentMap::sample(const Vector3f& dir) const
{
    if (!valid()) return Vector3f(0.0f);

    Vector3f d = normalize(dir);
    float u = 0.0f;
    float v = 0.0f;
    directionToUV(d, u, v);

    float fx = u * static_cast<float>(w - 1);
    float fy = v * static_cast<float>(h - 1);

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    int x1 = x0 + 1;
    int y1 = std::min(y0 + 1, h - 1);

    float tx = fx - static_cast<float>(x0);
    float ty = fy - static_cast<float>(y0);

    Vector3f c00 = sampleTexel(x0, y0);
    Vector3f c10 = sampleTexel(x1, y0);
    Vector3f c01 = sampleTexel(x0, y1);
    Vector3f c11 = sampleTexel(x1, y1);

    Vector3f cx0 = lerpVec(c00, c10, tx);
    Vector3f cx1 = lerpVec(c01, c11, tx);
    return lerpVec(cx0, cx1, ty) * intensity;
}

void EnvironmentMap::directionToUV(const Vector3f& dir, float& u, float& v) const
{
    Vector3f d = normalize(rotateY(dir, -rotationYDegrees));
    float theta = std::acos(clamp(-1.0f, 1.0f, d.y));
    float phi = std::atan2(d.z, d.x);
    if (phi < 0.0f) phi += 2.0f * M_PI;

    u = wrap01(phi / (2.0f * M_PI));
    v = std::clamp(static_cast<float>(theta / M_PI), 0.0f, 1.0f);
}

void EnvironmentMap::buildImportanceDistribution()
{
    texelWeights.assign(w * h, 0.0f);
    marginalCdf.assign(h + 1, 0.0f);
    conditionalCdf.assign(h * (w + 1), 0.0f);
    totalWeight = 0.0f;

    for (int y = 0; y < h; ++y) {
        float theta = static_cast<float>(M_PI) * (static_cast<float>(y) + 0.5f) /
                      static_cast<float>(h);
        float sinTheta = std::sin(theta);
        float rowWeight = 0.0f;
        size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(w + 1);
        conditionalCdf[rowOffset] = 0.0f;

        for (int x = 0; x < w; ++x) {
            float weight = std::max(0.0f, luminance(sampleTexel(x, y))) * sinTheta;
            texelWeights[y * w + x] = weight;
            rowWeight += weight;
            conditionalCdf[rowOffset + x + 1] = rowWeight;
        }

        if (rowWeight > 0.0f) {
            for (int x = 1; x <= w; ++x) {
                conditionalCdf[rowOffset + x] /= rowWeight;
            }
        } else {
            for (int x = 1; x <= w; ++x) {
                conditionalCdf[rowOffset + x] =
                    static_cast<float>(x) / static_cast<float>(w);
            }
        }

        totalWeight += rowWeight;
        marginalCdf[y + 1] = totalWeight;
    }

    if (totalWeight > 0.0f) {
        for (int y = 1; y <= h; ++y) {
            marginalCdf[y] /= totalWeight;
        }
    } else {
        for (int y = 1; y <= h; ++y) {
            marginalCdf[y] = static_cast<float>(y) / static_cast<float>(h);
        }
    }
}

bool EnvironmentMap::sampleDirection(Vector3f& dir, Vector3f& radiance, float& pdfOut) const
{
    dir = Vector3f(0.0f);
    radiance = Vector3f(0.0f);
    pdfOut = 0.0f;
    if (!valid() || totalWeight <= 0.0f) return false;

    float ry = get_random_float();
    auto rowIt = std::lower_bound(marginalCdf.begin() + 1, marginalCdf.end(), ry);
    int y = static_cast<int>(std::distance(marginalCdf.begin() + 1, rowIt));
    y = std::clamp(y, 0, h - 1);

    size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(w + 1);
    float rx = get_random_float();
    auto colBegin = conditionalCdf.begin() + static_cast<std::ptrdiff_t>(rowOffset + 1);
    auto colEnd = colBegin + w;
    auto colIt = std::lower_bound(colBegin, colEnd, rx);
    int x = static_cast<int>(std::distance(colBegin, colIt));
    x = std::clamp(x, 0, w - 1);

    float u = (static_cast<float>(x) + get_random_float()) / static_cast<float>(w);
    float v = (static_cast<float>(y) + get_random_float()) / static_cast<float>(h);
    float theta = v * static_cast<float>(M_PI);
    float phi = u * 2.0f * static_cast<float>(M_PI);
    float sinTheta = std::sin(theta);
    float cosTheta = std::cos(theta);
    dir = rotateY(Vector3f(std::cos(phi) * sinTheta,
                           cosTheta,
                           std::sin(phi) * sinTheta),
                  rotationYDegrees);
    dir = normalize(dir);
    radiance = sample(dir);
    pdfOut = pdf(dir);
    return pdfOut > 0.0f;
}

float EnvironmentMap::pdf(const Vector3f& dir) const
{
    if (!valid() || totalWeight <= 0.0f) return 0.0f;

    float u = 0.0f;
    float v = 0.0f;
    directionToUV(dir, u, v);

    int x = std::clamp(static_cast<int>(u * static_cast<float>(w)), 0, w - 1);
    int y = std::clamp(static_cast<int>(v * static_cast<float>(h)), 0, h - 1);
    float theta = v * static_cast<float>(M_PI);
    float sinTheta = std::sin(theta);
    if (sinTheta <= 1e-5f) return 0.0f;

    float pTexel = texelWeights[y * w + x] / totalWeight;
    return pTexel * static_cast<float>(w * h) /
           (2.0f * static_cast<float>(M_PI) * static_cast<float>(M_PI) * sinTheta);
}
