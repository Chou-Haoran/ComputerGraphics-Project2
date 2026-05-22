#include "ImageIO.hpp"
#include "global.hpp"
#include "stb_image_write.h"
#include <cstdio>
#include <cmath>
#include <vector>

namespace ImageIO {

namespace {

unsigned char encodeDisplay(float x)
{
    const float exposed = std::max(x, 0.0f) * 0.85f;
    const float mapped = (exposed * (2.51f * exposed + 0.03f)) /
                         (exposed * (2.43f * exposed + 0.59f) + 0.14f);
    const float gammaEncoded = std::pow(clamp(0.0f, 1.0f, mapped), 1.0f / 2.2f);
    return static_cast<unsigned char>(255.0f * gammaEncoded);
}

} // namespace

void writePPM(const std::string& path,
              const std::vector<Vector3f>& fb,
              int w, int h)
{
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        std::fprintf(stderr, "ImageIO::writePPM: failed to open %s\n", path.c_str());
        return;
    }
    std::fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        unsigned char rgb[3];
        rgb[0] = encodeDisplay(fb[i].x);
        rgb[1] = encodeDisplay(fb[i].y);
        rgb[2] = encodeDisplay(fb[i].z);
        std::fwrite(rgb, 1, 3, fp);
    }
    std::fclose(fp);
}

void writePNG(const std::string& path,
              const std::vector<Vector3f>& fb,
              int w, int h)
{
    std::vector<unsigned char> bytes(static_cast<size_t>(w) * h * 3);
    for (int i = 0; i < w * h; ++i) {
        bytes[i * 3 + 0] = encodeDisplay(fb[i].x);
        bytes[i * 3 + 1] = encodeDisplay(fb[i].y);
        bytes[i * 3 + 2] = encodeDisplay(fb[i].z);
    }

    if (stbi_write_png(path.c_str(), w, h, 3, bytes.data(), w * 3) == 0) {
        std::fprintf(stderr, "ImageIO::writePNG: failed to write %s\n", path.c_str());
    }
}

} // namespace ImageIO
