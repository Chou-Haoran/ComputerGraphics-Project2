#ifndef PROJECT2_SCENE_LOADER_H
#define PROJECT2_SCENE_LOADER_H

#include "Scene.hpp"
#include "Camera.hpp"
#include "EnvironmentMap.hpp"
#include "LightSpec.hpp"
#include "Material.hpp"
#include "Texture.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Declarative description of a Project2 scene. The `.scene` text file
// alongside the .obj exports authors these values; SceneLoader::load
// turns them into concrete Material / MeshTriangle / Camera /
// EnvironmentMap instances owned by LoadedScene.

struct MeshSpec {
    std::string  objPath;                 // relative to the scene directory
    std::optional<std::string>  material;
    std::optional<Vector3f>     color;
    Vector3f     offset   = {0, 0, 0};
    Vector3f     rotation = {0, 0, 0};    // TODO: MeshTriangle ignores this
    Vector3f     scale    = {1, 1, 1};    // TODO: MeshTriangle ignores this
    std::optional<std::string>  materialMap;  // per-MTL-name overrides
    std::optional<float>        ior;
    std::optional<float>        dispersion;
    std::optional<float>        metallic;
    std::optional<float>        roughness;
    std::optional<Vector3f>     emission; // non-zero ⇒ self-illuminating mesh
    std::optional<std::string>  diffuseTex;   // optional path relative to scene dir
    std::optional<std::string>  metallicTex;
    std::optional<std::string>  normalTex;
    std::optional<std::string>  bumpTex;
    std::optional<std::string>  roughnessTex;
    bool                       roughnessTexIsGloss = false;
    std::optional<float>       bumpScale;
    std::optional<float>       shadowTransmission;
    std::optional<Vector3f>    shadowTint;
    std::optional<float>       roughnessVariation;
    std::optional<std::string> displacementTex;
    std::optional<float>       displacementScale;
    // When true the OBJ's per-vertex normals are ignored and each triangle
    // uses its geometric face normal. Gives crisp facet edges on gems and
    // other hard-edge meshes that were exported with smoothing turned on.
    bool                       flatNormals = false;
};

struct CameraSpec {
    Vector3f position  = { 0.0f, 0.45f, -1.10f};
    Vector3f lookAt    = { 0.0f, 0.10f,  0.00f};
    Vector3f up        = { 0.0f, 1.00f,  0.00f};
    float    fovDeg    = 35.0f;
    float    aperture  = 0.0f;
    float    focusDist = 1.0f;
};

struct SceneDescriptor {
    std::vector<MeshSpec>  meshes;
    std::vector<LightSpec> lights;
    CameraSpec             camera;

    std::string envmapPath;
    float       envmapIntensity = 1.0f;
    float       envmapRotationY = 0.0f;

    int      width  = 800;
    int      height = 600;
    int      spp    = 128;
    int      maxDepth = 8;
    bool     shadowsEnabled = true;
    bool     adaptiveSamplingEnabled = true;
    int      adaptiveMinSpp = 16;
    int      adaptiveBatchSize = 8;
    float    adaptiveThreshold = 0.08f;
    bool     denoiseEnabled = true;
    int      denoiseRadius = 3;
    float    denoiseSigmaSpatial = 2.0f;
    float    denoiseSigmaColor = 0.15f;
    float    denoiseSigmaNormal = 0.2f;
    float    denoiseSigmaAlbedo = 0.25f;
    float    exposure = 1.0f;
    bool     toneMap = false;
    Vector3f background = {0.0f, 0.0f, 0.0f};
    int      causticPhotons = 0;            // 0 disables caustic photon mapping
    float    causticGatherRadius = 0.006f;  // photon-map lookup radius
    float    causticBoost = 1.0f;           // artistic multiplier on caustic energy
};

// CLI / runtime knobs that win over the .scene file when set. Built by
// main.cpp from argv so callers can override any field without editing
// the .scene file.
struct LoadOverrides {
    std::optional<int>         width;
    std::optional<int>         height;
    std::optional<int>         spp;
    std::optional<bool>        shadows;
    std::optional<std::string> envmapPath;
    std::optional<float>       aperture;
    std::optional<float>       focusDist;
    std::optional<std::string> outputName;   // CLI override for outputBaseName
};

// Bundles every owned heap resource so main.cpp keeps it alive until
// the renderer is done.
struct LoadedScene {
    std::unique_ptr<Scene>           scene;
    std::unique_ptr<Camera>          camera;
    std::unique_ptr<EnvironmentMap>  envmap;
    std::vector<std::unique_ptr<Material>> materials;
    std::vector<std::unique_ptr<Texture>>  textures;
    std::vector<std::unique_ptr<Object>>   objects;
};

class SceneLoader {
public:
    // Parses `<sceneDir>/<viewName>.scene` and builds the scene + camera.
    // Falls back to "load every .obj in sceneDir with default DIFFUSE
    // material" when no .scene file is present (smoke test friendly).
    static LoadedScene load(const std::string& sceneDir,
                            const std::string& viewName,
                            const LoadOverrides& overrides = {});
};

#endif // PROJECT2_SCENE_LOADER_H
