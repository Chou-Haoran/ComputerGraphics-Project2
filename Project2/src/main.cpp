#include "Camera.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "SceneLoader.hpp"
#include "global.hpp"

#include <chrono>
#include <iostream>
#include <string>

// Globals required by the migrated Task-2 modules.
float EPSILON    = 1e-3f;
bool  ENABLE_BVH = true;

namespace {

struct CliArgs {
    std::string   sceneDir = "assets/models/desktop_scene";
    std::string   viewName = "front";
    LoadOverrides overrides;
};

void printUsage(const char* prog) {
    std::cerr <<
        "Usage: " << prog << " <scene_dir> <view_name> [WxH] [spp] [flags]\n"
        "  <scene_dir>   directory under assets/models/ holding .obj exports\n"
        "                and an optional <view_name>.scene config\n"
        "  <view_name>   short tag, e.g. 'front' or 'side' — drives output filename\n"
        "  WxH           output resolution (default 800x600, PDF requires >=800x600)\n"
        "  spp           samples per pixel (default 128)\n"
        "Flags (any order, after positionals):\n"
        "  --no-shadow              disable shadow-ray tests              (PDF iv)\n"
        "  --envmap=path/to/sky.hdr enable image-based lighting           (PDF vi)\n"
        "  --aperture=<f>           thin-lens DOF aperture (0 = pinhole)  (PDF i)\n"
        "  --focus=<f>              focus distance for DOF\n";
}

bool parseArgs(int argc, char** argv, CliArgs& a) {
    if (argc < 3) return false;
    a.sceneDir = argv[1];
    a.viewName = argv[2];
    int p = 3;

    if (p < argc && argv[p][0] != '-') {
        std::string res = argv[p++];
        auto x = res.find('x');
        if (x == std::string::npos) return false;
        a.overrides.width  = std::stoi(res.substr(0, x));
        a.overrides.height = std::stoi(res.substr(x + 1));
    }
    if (p < argc && argv[p][0] != '-') {
        a.overrides.spp = std::stoi(argv[p++]);
    }
    for (int i = p; i < argc; ++i) {
        std::string s = argv[i];
        if      (s == "--no-shadow")              a.overrides.shadows  = false;
        else if (s.rfind("--envmap=", 0) == 0)    a.overrides.envmapPath = s.substr(9);
        else if (s.rfind("--aperture=", 0) == 0)  a.overrides.aperture = std::stof(s.substr(11));
        else if (s.rfind("--focus=", 0) == 0)     a.overrides.focusDist = std::stof(s.substr(8));
        else if (s.rfind("--output=", 0) == 0)    a.overrides.outputName = s.substr(9);
        else { std::cerr << "Unknown flag: " << s << "\n"; return false; }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage(argv[0]);
        return 1;
    }

    auto loaded = SceneLoader::load(args.sceneDir, args.viewName, args.overrides);
    if (!loaded.scene || !loaded.camera) {
        std::cerr << "SceneLoader returned an empty scene.\n";
        return 2;
    }

    // SceneLoader already applied all overrides, so scene/camera are in
    // their final shape. Resolution override → camera aspect already
    // matches; nothing more to splice in.

    Renderer r;
    auto t0 = std::chrono::system_clock::now();
    r.Render(*loaded.scene, *loaded.camera);
    auto t1 = std::chrono::system_clock::now();

    auto secs = std::chrono::duration_cast<std::chrono::seconds>(t1 - t0).count();
    std::cout << "Render complete in " << secs << " s. "
              << "Output: " << loaded.scene->outputBaseName << ".ppm/.png\n";
    return 0;
}
