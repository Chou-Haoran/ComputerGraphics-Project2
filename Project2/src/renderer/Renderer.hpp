#pragma once
#include "Scene.hpp"

class Camera;

class Renderer {
public:
    // Renders `scene` through `camera`. Resolution and spp come from the
    // scene. Writes <scene.outputBaseName>.ppm (and, when ImageIO::writePNG
    // is wired up, <outputBaseName>.png).
    void Render(Scene& scene, const Camera& camera);
};
