#ifndef PROJECT2_TEXTURE_H
#define PROJECT2_TEXTURE_H

#include <algorithm>
#include "Vector.hpp"
#include <string>
#include <vector>

// 2D image texture sampled by (u, v) in [0, 1]. Powers PDF requirement (iii)
// "object surface materials or texture maps" — wood grain on the desk,
// paper grain, typewriter keycap labels, dial digits on the rotary phone.
class Texture {
public:
    Texture() = default;
    ~Texture() = default;

    bool load(const std::string& path);

    Vector3f sample(float u, float v) const;
    Vector3f sample(float u, float v, float lod) const;
    Vector3f sampleAnisotropic(float u, float v,
                               float footprintU, float footprintV,
                               int maxSamples = 8) const;
    Vector3f sampleLevel(float u, float v, int level) const;
    float    sampleScalar(float u, float v) const;
    float    sampleScalar(float u, float v, float lod) const;

    bool valid() const { return !mips.empty() && !mips[0].data.empty(); }
    int  width()  const { return w; }
    int  height() const { return h; }
    int  mipLevels() const { return static_cast<int>(mips.size()); }
    float maxLod() const { return static_cast<float>(std::max(0, mipLevels() - 1)); }

private:
    struct MipLevel {
        std::vector<unsigned char> data;
        int width = 0;
        int height = 0;
    };

    bool loadPPM(const std::string& path);
    void reset();
    void assign(std::vector<unsigned char>&& pixels, int width, int height, int numChannels);
    void buildMipChain();
    Vector3f sampleTexel(const MipLevel& level, int x, int y) const;
    Vector3f sampleBilinear(const MipLevel& level, float u, float v) const;

    std::vector<MipLevel> mips;
    int                   w = 0;
    int                   h = 0;
    int                   channels = 0;
};

#endif // PROJECT2_TEXTURE_H
