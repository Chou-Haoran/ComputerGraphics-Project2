#pragma once

#include <memory>
#include <string>
#include <vector>
#include "BVH.hpp"
#include "EnvironmentMap.hpp"
#include "Light.hpp"
#include "Object.hpp"
#include "Ray.hpp"
#include "Vector.hpp"

class Camera; // forward — Renderer pulls the camera in, Scene only stores ptr.

class Scene {
public:
    int      width  = 800;
    int      height = 600;
    double   fov    = 40;                        // legacy fallback when camera == nullptr
    Vector3f backgroundColor = Vector3f(0.235294f, 0.67451f, 0.843137f);
    int      maxDepth = 8;
    float    RussianRoulette = 0.8f;
    int      spp = 128;
    bool     adaptiveSamplingEnabled = true;
    int      adaptiveMinSpp = 16;
    int      adaptiveBatchSize = 8;
    float    adaptiveThreshold = 0.08f;
    float    adaptiveLuminanceFloor = 0.03f;
    bool     denoiseEnabled = true;
    int      denoiseRadius = 3;
    float    denoiseSigmaSpatial = 2.0f;
    float    denoiseSigmaColor = 0.15f;
    float    denoiseSigmaNormal = 0.2f;
    float    denoiseSigmaAlbedo = 0.25f;
    float    exposure = 1.0f;
    bool     toneMap = true;
    std::string outputBaseName = "render";

    // PDF user-control requirement (iv): shadow toggle. When false the
    // direct-light contribution skips the shadow-ray visibility test and
    // assumes the light is always visible.
    bool shadowsEnabled = true;

    // PDF user-control requirement (vi): env-map IBL. Owned externally.
    const EnvironmentMap* envmap = nullptr;

    // PDF user-control requirement (i): camera. Owned externally.
    const Camera* camera = nullptr;

    Scene(int w, int h) : width(w), height(h) {}

    void Add(Object* object) { objects.push_back(object); }
    void AddLight(std::unique_ptr<Light> light);

    const std::vector<Object*>& get_objects() const { return objects; }
    Intersection intersect(const Ray& ray) const;
    Intersection intersectPrimary(const Ray& ray) const;
    BVHAccel* bvh = nullptr;
    void buildBVH();
    void buildPrimaryRayAccel();
    // Bits OR'd into *pathFlags during traversal so the renderer can decide
    // post-hoc whether firefly clamping is safe for the sample. Currently
    // only TouchedDispersion is used to keep caustic samples intact.
    enum PathFlag : int {
        PathFlagTouchedDispersion = 1 << 0
    };

    Vector3f castRay(const Ray& ray, int depth, int* pathFlags = nullptr) const;
    void sampleLight(Intersection& pos, float& pdf) const;
    int sampleAnalyticLight(float u, float& pickPdf) const;

    std::vector<Object*> objects;
    std::vector<Object*> primaryRayObjects;
    std::vector<std::unique_ptr<Light>> analyticLights;
    std::vector<float> analyticLightCdf;
    float analyticLightWeightSum = 0.0f;
    BVHAccel* primaryBvh = nullptr;
};
