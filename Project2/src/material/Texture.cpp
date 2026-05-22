#include "Texture.hpp"

#include "global.hpp"
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

float wrapUnit(float x)
{
    x = x - std::floor(x);
    return x < 0.0f ? x + 1.0f : x;
}

Vector3f lerpVec(const Vector3f& a, const Vector3f& b, float t)
{
    return a * (1.0f - t) + b * t;
}

bool nextToken(std::istream& in, std::string& tok)
{
    tok.clear();
    while (in >> tok) {
        if (!tok.empty() && tok[0] == '#') {
            std::string discard;
            std::getline(in, discard);
            continue;
        }
        return true;
    }
    return false;
}

} // namespace

void Texture::reset()
{
    mips.clear();
    w = 0;
    h = 0;
    channels = 0;
}

void Texture::assign(std::vector<unsigned char>&& pixels,
                     int width,
                     int height,
                     int numChannels)
{
    reset();
    if (pixels.empty() || width <= 0 || height <= 0 || numChannels <= 0) return;

    MipLevel base;
    base.data = std::move(pixels);
    base.width = width;
    base.height = height;
    mips.push_back(std::move(base));
    w = width;
    h = height;
    channels = numChannels;
    buildMipChain();
}

bool Texture::load(const std::string& path)
{
    reset();

    if (loadPPM(path)) return true;

    int width = 0;
    int height = 0;
    int numChannels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &numChannels, 0);
    if (!pixels) {
        std::cerr << "Texture::load: failed to load " << path
                  << " (" << stbi_failure_reason() << ")\n";
        return false;
    }

    std::vector<unsigned char> owned(
        pixels,
        pixels + static_cast<size_t>(width) * height * numChannels
    );
    stbi_image_free(pixels);
    assign(std::move(owned), width, height, numChannels);
    return valid();
}

bool Texture::loadPPM(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    std::string magic;
    if (!nextToken(in, magic)) return false;
    if (magic != "P6" && magic != "P3") return false;

    std::string tok;
    if (!nextToken(in, tok)) return false;
    int width = std::stoi(tok);
    if (!nextToken(in, tok)) return false;
    int height = std::stoi(tok);
    if (!nextToken(in, tok)) return false;
    int maxValue = std::stoi(tok);

    if (width <= 0 || height <= 0 || maxValue <= 0) return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    if (magic == "P6") {
        in.get();
        std::vector<unsigned char> raw(pixels.size());
        in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
        if (in.gcount() != static_cast<std::streamsize>(raw.size())) return false;

        if (maxValue == 255) {
            pixels = std::move(raw);
        } else {
            for (size_t i = 0; i < raw.size(); ++i) {
                pixels[i] = static_cast<unsigned char>(
                    std::clamp((int(raw[i]) * 255 + maxValue / 2) / maxValue, 0, 255));
            }
        }
    } else {
        for (size_t i = 0; i < pixels.size(); ++i) {
            if (!nextToken(in, tok)) return false;
            int v = std::stoi(tok);
            pixels[i] = static_cast<unsigned char>(
                std::clamp((v * 255 + maxValue / 2) / maxValue, 0, 255));
        }
    }

    assign(std::move(pixels), width, height, 3);
    return valid();
}

void Texture::buildMipChain()
{
    while (!mips.empty()) {
        const MipLevel& prev = mips.back();
        if (prev.width == 1 && prev.height == 1) break;

        MipLevel next;
        next.width = std::max(1, prev.width / 2);
        next.height = std::max(1, prev.height / 2);
        next.data.resize(static_cast<size_t>(next.width) * next.height * channels);

        for (int y = 0; y < next.height; ++y) {
            for (int x = 0; x < next.width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    int accum = 0;
                    int count = 0;
                    for (int oy = 0; oy < 2; ++oy) {
                        for (int ox = 0; ox < 2; ++ox) {
                            const int srcX = std::min(prev.width - 1, x * 2 + ox);
                            const int srcY = std::min(prev.height - 1, y * 2 + oy);
                            const int srcIdx = (srcY * prev.width + srcX) * channels + c;
                            accum += prev.data[static_cast<size_t>(srcIdx)];
                            ++count;
                        }
                    }

                    const int dstIdx = (y * next.width + x) * channels + c;
                    next.data[static_cast<size_t>(dstIdx)] =
                        static_cast<unsigned char>((accum + count / 2) / count);
                }
            }
        }

        mips.push_back(std::move(next));
    }
}

Vector3f Texture::sampleTexel(const MipLevel& level, int x, int y) const
{
    x = std::clamp(x, 0, level.width - 1);
    y = std::clamp(y, 0, level.height - 1);

    const int idx = (y * level.width + x) * channels;
    constexpr float inv255 = 1.0f / 255.0f;

    if (channels == 1) {
        const float c = level.data[static_cast<size_t>(idx)] * inv255;
        return Vector3f(c, c, c);
    }

    if (channels == 2) {
        const float c = level.data[static_cast<size_t>(idx)] * inv255;
        return Vector3f(c, c, c);
    }

    return Vector3f(level.data[static_cast<size_t>(idx + 0)] * inv255,
                    level.data[static_cast<size_t>(idx + 1)] * inv255,
                    level.data[static_cast<size_t>(idx + 2)] * inv255);
}

Vector3f Texture::sampleBilinear(const MipLevel& level, float u, float v) const
{
    if (!valid()) return Vector3f(1.0f, 1.0f, 1.0f);

    u = wrapUnit(u);
    v = wrapUnit(v);

    const float fx = u * static_cast<float>(level.width - 1);
    const float fy = (1.0f - v) * static_cast<float>(level.height - 1);

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(x0 + 1, level.width - 1);
    const int y1 = std::min(y0 + 1, level.height - 1);

    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const Vector3f c00 = sampleTexel(level, x0, y0);
    const Vector3f c10 = sampleTexel(level, x1, y0);
    const Vector3f c01 = sampleTexel(level, x0, y1);
    const Vector3f c11 = sampleTexel(level, x1, y1);

    const Vector3f cx0 = lerpVec(c00, c10, tx);
    const Vector3f cx1 = lerpVec(c01, c11, tx);
    return lerpVec(cx0, cx1, ty);
}

Vector3f Texture::sample(float u, float v) const
{
    return sample(u, v, 0.0f);
}

Vector3f Texture::sampleLevel(float u, float v, int level) const
{
    if (!valid()) return Vector3f(1.0f, 1.0f, 1.0f);
    const int clampedLevel = std::clamp(level, 0, mipLevels() - 1);
    return sampleBilinear(mips[static_cast<size_t>(clampedLevel)], u, v);
}

Vector3f Texture::sample(float u, float v, float lod) const
{
    if (!valid()) return Vector3f(1.0f, 1.0f, 1.0f);

    const float clampedLod = clamp(0.0f, maxLod(), lod);
    const int level0 = static_cast<int>(std::floor(clampedLod));
    const int level1 = std::min(level0 + 1, mipLevels() - 1);
    const float t = clampedLod - static_cast<float>(level0);

    const Vector3f c0 = sampleLevel(u, v, level0);
    const Vector3f c1 = sampleLevel(u, v, level1);
    return lerpVec(c0, c1, t);
}

Vector3f Texture::sampleAnisotropic(float u, float v,
                                    float footprintU, float footprintV,
                                    int maxSamples) const
{
    if (!valid()) return Vector3f(1.0f, 1.0f, 1.0f);

    const float rhoU = std::fabs(footprintU) * static_cast<float>(width());
    const float rhoV = std::fabs(footprintV) * static_cast<float>(height());
    const float majorRho = std::max(rhoU, rhoV);
    const float minorRho = std::max(std::min(rhoU, rhoV), 1.0f);
    const float anisotropy = majorRho / minorRho;

    if (majorRho <= 1.0f || anisotropy <= 1.5f || maxSamples <= 1) {
        const float lod = majorRho > 1.0f ? std::log2(majorRho) : 0.0f;
        return sample(u, v, lod);
    }

    const bool majorIsU = rhoU >= rhoV;
    const float minorLod = std::log2(minorRho);
    const int taps = std::clamp(static_cast<int>(std::ceil(anisotropy)), 2, maxSamples);

    Vector3f accum(0.0f);
    for (int i = 0; i < taps; ++i) {
        const float offset = ((static_cast<float>(i) + 0.5f) / static_cast<float>(taps)) - 0.5f;
        const float sampleU = majorIsU ? u + offset * footprintU : u;
        const float sampleV = majorIsU ? v : v + offset * footprintV;
        accum += sample(sampleU, sampleV, minorLod);
    }
    return accum / static_cast<float>(taps);
}

float Texture::sampleScalar(float u, float v) const
{
    return sampleScalar(u, v, 0.0f);
}

float Texture::sampleScalar(float u, float v, float lod) const
{
    const Vector3f rgb = sample(u, v, lod);
    return std::clamp(0.2126f * rgb.x + 0.7152f * rgb.y + 0.0722f * rgb.z, 0.0f, 1.0f);
}
