#include "Texture.hpp"

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

Texture::~Texture()
{
    reset();
}

void Texture::reset()
{
    delete[] data;
    data = nullptr;
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

    data = new unsigned char[pixels.size()];
    std::copy(pixels.begin(), pixels.end(), data);
    w = width;
    h = height;
    channels = numChannels;
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

Vector3f Texture::sampleTexel(int x, int y) const
{
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);

    int idx = (y * w + x) * channels;
    constexpr float inv255 = 1.0f / 255.0f;

    if (channels == 1) {
        float c = data[idx] * inv255;
        return Vector3f(c, c, c);
    }

    if (channels == 2) {
        float c = data[idx] * inv255;
        return Vector3f(c, c, c);
    }

    return Vector3f(data[idx + 0] * inv255,
                    data[idx + 1] * inv255,
                    data[idx + 2] * inv255);
}

Vector3f Texture::sample(float u, float v) const
{
    if (!valid()) return Vector3f(1.0f, 1.0f, 1.0f);

    u = wrapUnit(u);
    v = wrapUnit(v);

    float fx = u * static_cast<float>(w - 1);
    float fy = (1.0f - v) * static_cast<float>(h - 1);

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    int x1 = std::min(x0 + 1, w - 1);
    int y1 = std::min(y0 + 1, h - 1);

    float tx = fx - static_cast<float>(x0);
    float ty = fy - static_cast<float>(y0);

    Vector3f c00 = sampleTexel(x0, y0);
    Vector3f c10 = sampleTexel(x1, y0);
    Vector3f c01 = sampleTexel(x0, y1);
    Vector3f c11 = sampleTexel(x1, y1);

    Vector3f cx0 = lerpVec(c00, c10, tx);
    Vector3f cx1 = lerpVec(c01, c11, tx);
    return lerpVec(cx0, cx1, ty);
}

float Texture::sampleScalar(float u, float v) const
{
    Vector3f rgb = sample(u, v);
    return std::clamp(0.2126f * rgb.x + 0.7152f * rgb.y + 0.0722f * rgb.z, 0.0f, 1.0f);
}
