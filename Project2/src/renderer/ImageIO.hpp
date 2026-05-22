#ifndef PROJECT2_IMAGE_IO_H
#define PROJECT2_IMAGE_IO_H

#include "Vector.hpp"
#include <string>
#include <vector>

namespace ImageIO {

// Writes the framebuffer with Reinhard tone mapping followed by 2.2 gamma.
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
