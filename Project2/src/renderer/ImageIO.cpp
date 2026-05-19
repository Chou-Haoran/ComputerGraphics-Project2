#include "ImageIO.hpp"
#include "global.hpp"
#include "stb_image_write.h"
#include <cstdio>
#include <cmath>
#include <vector>

namespace ImageIO {

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
        rgb[0] = (unsigned char)(255 * std::pow(clamp(0, 1, fb[i].x), 0.6f));
        rgb[1] = (unsigned char)(255 * std::pow(clamp(0, 1, fb[i].y), 0.6f));
        rgb[2] = (unsigned char)(255 * std::pow(clamp(0, 1, fb[i].z), 0.6f));
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
        bytes[i * 3 + 0] = static_cast<unsigned char>(255 * std::pow(clamp(0, 1, fb[i].x), 0.6f));
        bytes[i * 3 + 1] = static_cast<unsigned char>(255 * std::pow(clamp(0, 1, fb[i].y), 0.6f));
        bytes[i * 3 + 2] = static_cast<unsigned char>(255 * std::pow(clamp(0, 1, fb[i].z), 0.6f));
    }

    if (stbi_write_png(path.c_str(), w, h, 3, bytes.data(), w * 3) == 0) {
        std::fprintf(stderr, "ImageIO::writePNG: failed to write %s\n", path.c_str());
    }
}

} // namespace ImageIO
