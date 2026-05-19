#ifndef PROJECT2_TEXTURE_H
#define PROJECT2_TEXTURE_H

#include "Vector.hpp"
#include <string>
#include <vector>

// 2D image texture sampled by (u, v) in [0, 1]. Powers PDF requirement (iii)
// "object surface materials or texture maps" — wood grain on the desk,
// paper grain, typewriter keycap labels, dial digits on the rotary phone.
class Texture {
public:
    Texture() = default;
    ~Texture();

    bool load(const std::string& path);

    Vector3f sample(float u, float v) const;
    float    sampleScalar(float u, float v) const;

    bool valid() const { return data != nullptr; }
    int  width()  const { return w; }
    int  height() const { return h; }

private:
    bool loadPPM(const std::string& path);
    void reset();
    void assign(std::vector<unsigned char>&& pixels, int width, int height, int numChannels);
    Vector3f sampleTexel(int x, int y) const;

    unsigned char* data = nullptr;
    int            w = 0;
    int            h = 0;
    int            channels = 0;
};

#endif // PROJECT2_TEXTURE_H
