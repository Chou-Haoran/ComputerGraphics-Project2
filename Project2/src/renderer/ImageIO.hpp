#ifndef PROJECT2_IMAGE_IO_H
#define PROJECT2_IMAGE_IO_H

#include "Vector.hpp"
#include <string>
#include <vector>

namespace ImageIO {

// Writes a binary P6 PPM with sRGB-ish gamma (exponent 0.6, mirroring Task-2).
void writePPM(const std::string& path,
              const std::vector<Vector3f>& fb,
              int w, int h);

// TODO: native PNG output via stb_image_write to remove the dependency on
// macOS `sips`. Falls back to invoking `sips` on Apple platforms for now so
// the pipeline produces a .png alongside the .ppm out-of-the-box.
void writePNG(const std::string& path,
              const std::vector<Vector3f>& fb,
              int w, int h);

} // namespace ImageIO

#endif // PROJECT2_IMAGE_IO_H
