#include "SceneLoader.hpp"
#include "AreaRectLight.hpp"
#include "AreaDiskLight.hpp"
#include "AreaSphereLight.hpp"
#include "DirectionalLight.hpp"
#include "LightType.hpp"
#include "MaterialFactory.hpp"
#include "MeshLight.hpp"
#include "PointLight.hpp"
#include "SpotLight.hpp"
#include "Sphere.hpp"
#include "Triangle.hpp"
#include "global.hpp"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ===== Tokenizer =========================================================

namespace {

bool isAreaLightType(LightType type)
{
    return type == LightType::AreaRect ||
           type == LightType::AreaDisk ||
           type == LightType::AreaSphere;
}

Vector3f lightNormalFromDirection(const Vector3f& direction)
{
    Vector3f normal = normalize(direction);
    if (dotProduct(normal, normal) <= EPSILON) {
        normal = Vector3f(0.0f, -1.0f, 0.0f);
    }
    return normal;
}

void buildLightFrame(const Vector3f& normal, Vector3f& tangent, Vector3f& bitangent)
{
    if (std::fabs(normal.y) > 0.999f) {
        tangent = Vector3f(1.0f, 0.0f, 0.0f);
    } else {
        tangent = normalize(crossProduct(Vector3f(0.0f, 1.0f, 0.0f), normal));
    }
    bitangent = normalize(crossProduct(normal, tangent));
}

struct Token {
    enum Kind { Ident, Number, String, Symbol, Eof };
    Kind        kind;
    std::string text;
    int         line;
};

struct MaterialOverrideSpec {
    std::optional<std::string>  type;
    std::optional<Vector3f>     color;
    std::optional<float>        ior;
    std::optional<float>        metallic;
    std::optional<float>        roughness;
    std::optional<float>        Kd;
    std::optional<float>        Ks;
    std::optional<Vector3f>     emission;
    std::optional<std::string>  diffuseTex;
    std::optional<std::string>  metallicTex;
    std::optional<std::string>  normalTex;
    std::optional<std::string>  bumpTex;
    std::optional<std::string>  roughnessTex;
    std::optional<bool>         roughnessTexIsGloss;
    std::optional<float>        bumpScale;
    std::optional<float>        shadowTransmission;
    std::optional<Vector3f>     shadowTint;
};

struct MaterialMapConfig {
    std::optional<std::string>  defaultType;
    std::optional<float>        defaultRoughnessMin;
    std::unordered_map<std::string, MaterialOverrideSpec> overrides;
    fs::path sourceDir;
};

bool isSymbolChar(char c) {
    return c == '{' || c == '}' || c == '=' || c == ',';
}

std::vector<Token> tokenize(std::istream& in)
{
    std::vector<Token> out;
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        size_t i = 0;
        while (i < line.size()) {
            char c = line[i];
            if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
            if (c == '#') break;                                    // # comment
            if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') break; // // comment
            if (c == '"') {                                         // quoted string
                ++i;
                size_t start = i;
                while (i < line.size() && line[i] != '"') ++i;
                out.push_back({Token::String, line.substr(start, i - start), lineno});
                if (i < line.size()) ++i;
                continue;
            }
            if (isSymbolChar(c)) {
                out.push_back({Token::Symbol, std::string(1, c), lineno});
                ++i;
                continue;
            }
            size_t start = i;
            while (i < line.size() &&
                   !std::isspace(static_cast<unsigned char>(line[i])) &&
                   !isSymbolChar(line[i]) &&
                   line[i] != '#' && line[i] != '"') {
                ++i;
            }
            std::string tok = line.substr(start, i - start);
            char head = tok.empty() ? 0 : tok[0];
            bool numLike = (std::isdigit(static_cast<unsigned char>(head)) != 0) ||
                           head == '-' || head == '+' || head == '.';
            // Pure '-' / '+' (rare) stay ident-classed; parseNum will reject.
            out.push_back({numLike ? Token::Number : Token::Ident, tok, lineno});
        }
    }
    out.push_back({Token::Eof, "", lineno});
    return out;
}

// ===== Parser ============================================================

class Parser {
public:
    Parser(std::vector<Token> t, std::string srcName)
        : toks(std::move(t)), name(std::move(srcName)) {}

    void parse(SceneDescriptor& d) {
        while (peek().kind != Token::Eof) {
            if (peek().kind != Token::Ident) errorAt("expected block keyword");
            std::string kw = advance().text;
            if      (kw == "render") parseRender(d);
            else if (kw == "camera") parseCamera(d.camera);
            else if (kw == "envmap") parseEnvmap(d);
            else if (kw == "mesh")   parseMesh(d);
            else if (kw == "light")  parseLight(d);
            else errorAt("unknown top-level block: " + kw);
        }
    }

private:
    std::vector<Token> toks;
    std::string        name;
    size_t             pos = 0;

    const Token& peek() const   { return toks[pos]; }
    const Token& advance()      { return toks[pos++]; }
    bool match(Token::Kind k, const std::string& s = "") {
        if (peek().kind != k) return false;
        if (!s.empty() && peek().text != s) return false;
        ++pos;
        return true;
    }
    void expect(Token::Kind k, const std::string& s = "") {
        if (!match(k, s)) errorAt("expected '" + s + "'");
    }
    [[noreturn]] void errorAt(const std::string& msg) {
        throw std::runtime_error(name + ":" + std::to_string(peek().line) +
                                 ": " + msg + " (got '" + peek().text + "')");
    }

    float       parseNum()   {
        if (peek().kind != Token::Number) errorAt("expected number");
        return std::stof(advance().text);
    }
    int         parseInt()   { return static_cast<int>(parseNum()); }
    Vector3f    parseV3()    {
        float x = parseNum(); match(Token::Symbol, ",");
        float y = parseNum(); match(Token::Symbol, ",");
        float z = parseNum();
        return {x, y, z};
    }
    std::string parseStr()   {
        if (peek().kind != Token::String) errorAt("expected quoted string");
        return advance().text;
    }
    std::string parseIdent() {
        if (peek().kind != Token::Ident) errorAt("expected identifier");
        return advance().text;
    }
    bool        parseBool()  {
        std::string s = parseIdent();
        if (s == "true"  || s == "1") return true;
        if (s == "false" || s == "0") return false;
        errorAt("expected true/false");
    }
    std::string parseMaterialType() {
        std::string s = MaterialFactory::normalizeTypeName(parseIdent());
        if (!MaterialFactory::instance().isRegistered(s)) {
            errorAt("unknown material type: " + s);
        }
        return s;
    }
    LightType parseLightType() {
        std::string s = parseIdent();
        try {
            return parseLightTypeName(s);
        } catch (const std::invalid_argument& e) {
            errorAt(e.what());
        }
    }

    void parseRender(SceneDescriptor& d) {
        expect(Token::Symbol, "{");
        while (!match(Token::Symbol, "}")) {
            std::string f = parseIdent();
            expect(Token::Symbol, "=");
            if      (f == "width")   d.width   = parseInt();
            else if (f == "height")  d.height  = parseInt();
            else if (f == "spp")     d.spp     = parseInt();
            else if (f == "depth")   d.maxDepth = parseInt();
            else if (f == "shadows") d.shadowsEnabled = parseBool();
            else if (f == "adaptiveSampling") d.adaptiveSamplingEnabled = parseBool();
            else if (f == "adaptiveMinSpp")   d.adaptiveMinSpp = parseInt();
            else if (f == "adaptiveBatch")    d.adaptiveBatchSize = parseInt();
            else if (f == "adaptiveThreshold") d.adaptiveThreshold = parseNum();
            else if (f == "denoise") d.denoiseEnabled = parseBool();
            else if (f == "denoiseRadius") d.denoiseRadius = parseInt();
            else if (f == "denoiseSigmaSpatial") d.denoiseSigmaSpatial = parseNum();
            else if (f == "denoiseSigmaColor") d.denoiseSigmaColor = parseNum();
            else if (f == "denoiseSigmaNormal") d.denoiseSigmaNormal = parseNum();
            else if (f == "denoiseSigmaAlbedo") d.denoiseSigmaAlbedo = parseNum();
            else if (f == "bg")      d.background = parseV3();
            else errorAt("unknown render field: " + f);
        }
    }
    void parseCamera(CameraSpec& c) {
        expect(Token::Symbol, "{");
        while (!match(Token::Symbol, "}")) {
            std::string f = parseIdent();
            expect(Token::Symbol, "=");
            if      (f == "pos")      c.position  = parseV3();
            else if (f == "lookAt")   c.lookAt    = parseV3();
            else if (f == "up")       c.up        = parseV3();
            else if (f == "fov")      c.fovDeg    = parseNum();
            else if (f == "aperture") c.aperture  = parseNum();
            else if (f == "focus")    c.focusDist = parseNum();
            else errorAt("unknown camera field: " + f);
        }
    }
    void parseEnvmap(SceneDescriptor& d) {
        d.envmapPath = parseStr();
        if (match(Token::Symbol, "{")) {
            while (!match(Token::Symbol, "}")) {
                std::string f = parseIdent();
                expect(Token::Symbol, "=");
                if      (f == "intensity") d.envmapIntensity = parseNum();
                else if (f == "rotationY") d.envmapRotationY = parseNum();
                else errorAt("unknown envmap field: " + f);
            }
        }
    }
    void parseMesh(SceneDescriptor& d) {
        MeshSpec m;
        m.objPath = parseStr();
        expect(Token::Symbol, "{");
        while (!match(Token::Symbol, "}")) {
            std::string f = parseIdent();
            expect(Token::Symbol, "=");
            if      (f == "material")     m.material     = parseMaterialType();
            else if (f == "materialMap")  m.materialMap  = parseStr();
            else if (f == "color")        m.color        = parseV3();
            else if (f == "offset")       m.offset       = parseV3();
            else if (f == "rotation")     m.rotation     = parseV3();
            else if (f == "scale")        m.scale        = parseV3();
            else if (f == "ior")          m.ior          = parseNum();
            else if (f == "metallic")     m.metallic     = parseNum();
            else if (f == "roughness")    m.roughness    = parseNum();
            else if (f == "emission")     m.emission     = parseV3();
            else if (f == "diffuseTex")   m.diffuseTex   = parseStr();
            else if (f == "metallicTex")  m.metallicTex  = parseStr();
            else if (f == "normalTex")    m.normalTex    = parseStr();
            else if (f == "bumpTex")      m.bumpTex      = parseStr();
            else if (f == "roughnessTex") m.roughnessTex = parseStr();
            else if (f == "bumpScale")    m.bumpScale    = parseNum();
            else if (f == "shadowTransmission") m.shadowTransmission = parseNum();
            else if (f == "shadowTint")         m.shadowTint = parseV3();
            else errorAt("unknown mesh field: " + f);
        }
        d.meshes.push_back(std::move(m));
    }
    void parseLight(SceneDescriptor& d) {
        LightSpec L;
        expect(Token::Symbol, "{");
        while (!match(Token::Symbol, "}")) {
            std::string f = parseIdent();
            expect(Token::Symbol, "=");
            if      (f == "type")       L.type          = parseLightType();
            else if (f == "pos")        L.position      = parseV3();
            else if (f == "size")       L.size          = parseV3();
            else if (f == "dir")        L.direction     = parseV3();
            else if (f == "normal")     L.direction     = parseV3();
            else if (f == "emission")   L.emission      = parseV3();
            else if (f == "intensity")  L.intensity     = parseNum();
            else if (f == "radius")     L.radius        = parseNum();
            else if (f == "range")      L.range         = parseNum();
            else if (f == "innerAngle") L.innerAngleDeg = parseNum();
            else if (f == "outerAngle") L.outerAngleDeg = parseNum();
            else if (f == "enabled")    L.enabled       = parseBool();
            else if (f == "showModel")  L.showModel     = parseBool();
            else errorAt("unknown light field: " + f);
        }
        d.lights.push_back(L);
    }
};

class MaterialMapParser {
public:
    MaterialMapParser(std::vector<Token> t, std::string srcName)
        : toks(std::move(t)), name(std::move(srcName)) {}

    void parse(MaterialMapConfig& cfg) {
        while (peek().kind != Token::Eof) {
            if (peek().kind != Token::Ident) errorAt("expected block keyword");
            std::string kw = advance().text;
            if (kw == "materialMap") parseDefaults(cfg);
            else if (kw == "mtl")    parseOverride(cfg);
            else errorAt("unknown top-level block: " + kw);
        }
    }

private:
    std::vector<Token> toks;
    std::string        name;
    size_t             pos = 0;

    const Token& peek() const   { return toks[pos]; }
    const Token& advance()      { return toks[pos++]; }
    bool match(Token::Kind k, const std::string& s = "") {
        if (peek().kind != k) return false;
        if (!s.empty() && peek().text != s) return false;
        ++pos;
        return true;
    }
    void expect(Token::Kind k, const std::string& s = "") {
        if (!match(k, s)) errorAt("expected '" + s + "'");
    }
    [[noreturn]] void errorAt(const std::string& msg) {
        throw std::runtime_error(name + ":" + std::to_string(peek().line) +
                                 ": " + msg + " (got '" + peek().text + "')");
    }

    float       parseNum()   {
        if (peek().kind != Token::Number) errorAt("expected number");
        return std::stof(advance().text);
    }
    Vector3f    parseV3()    {
        float x = parseNum(); match(Token::Symbol, ",");
        float y = parseNum(); match(Token::Symbol, ",");
        float z = parseNum();
        return {x, y, z};
    }
    std::string parseStr()   {
        if (peek().kind != Token::String) errorAt("expected quoted string");
        return advance().text;
    }
    std::string parseIdent() {
        if (peek().kind != Token::Ident) errorAt("expected identifier");
        return advance().text;
    }
    bool        parseBool()  {
        std::string s = parseIdent();
        if (s == "true"  || s == "1") return true;
        if (s == "false" || s == "0") return false;
        errorAt("expected true/false");
    }
    std::string parseMaterialType() {
        std::string s = MaterialFactory::normalizeTypeName(parseIdent());
        if (!MaterialFactory::instance().isRegistered(s)) {
            errorAt("unknown material type: " + s);
        }
        return s;
    }

    void parseDefaults(MaterialMapConfig& cfg) {
        expect(Token::Symbol, "{");
        while (!match(Token::Symbol, "}")) {
            std::string f = parseIdent();
            expect(Token::Symbol, "=");
            if      (f == "defaultType")         cfg.defaultType = parseMaterialType();
            else if (f == "defaultRoughnessMin") cfg.defaultRoughnessMin = parseNum();
            else errorAt("unknown materialMap field: " + f);
        }
    }

    void parseOverride(MaterialMapConfig& cfg) {
        MaterialOverrideSpec spec;
        std::string materialName = parseStr();
        expect(Token::Symbol, "{");
        while (!match(Token::Symbol, "}")) {
            std::string f = parseIdent();
            expect(Token::Symbol, "=");
            if      (f == "type")                spec.type = parseMaterialType();
            else if (f == "color")               spec.color = parseV3();
            else if (f == "ior")                 spec.ior = parseNum();
            else if (f == "metallic")            spec.metallic = parseNum();
            else if (f == "roughness")           spec.roughness = parseNum();
            else if (f == "Kd")                  spec.Kd = parseNum();
            else if (f == "Ks")                  spec.Ks = parseNum();
            else if (f == "emission")            spec.emission = parseV3();
            else if (f == "diffuseTex")          spec.diffuseTex = parseStr();
            else if (f == "metallicTex")         spec.metallicTex = parseStr();
            else if (f == "normalTex")           spec.normalTex = parseStr();
            else if (f == "bumpTex")             spec.bumpTex = parseStr();
            else if (f == "roughnessTex")        spec.roughnessTex = parseStr();
            else if (f == "roughnessTexIsGloss") spec.roughnessTexIsGloss = parseBool();
            else if (f == "bumpScale")           spec.bumpScale = parseNum();
            else if (f == "shadowTransmission")  spec.shadowTransmission = parseNum();
            else if (f == "shadowTint")          spec.shadowTint = parseV3();
            else errorAt("unknown mtl field: " + f);
        }
        cfg.overrides[materialName] = std::move(spec);
    }
};

// ===== Helpers ===========================================================

fs::path resolveRelative(const fs::path& base, const std::string& rel) {
    if (rel.empty()) return {};
    fs::path p(rel);
    return p.is_absolute() ? p : (base / p);
}

void applyOverrides(SceneDescriptor& d, const LoadOverrides& o)
{
    if (o.width)      d.width            = *o.width;
    if (o.height)     d.height           = *o.height;
    if (o.spp)        d.spp              = *o.spp;
    if (o.shadows)    d.shadowsEnabled   = *o.shadows;
    if (o.envmapPath) d.envmapPath       = *o.envmapPath;
    if (o.aperture)   d.camera.aperture  = *o.aperture;
    if (o.focusDist)  d.camera.focusDist = *o.focusDist;
}

void fallbackDirectoryScan(const fs::path& sceneRoot, SceneDescriptor& d)
{
    if (!fs::exists(sceneRoot) || !fs::is_directory(sceneRoot)) return;
    for (auto& entry : fs::directory_iterator(sceneRoot)) {
        if (entry.path().extension() != ".obj") continue;
        MeshSpec m;
        m.objPath = entry.path().filename().string();
        bool looksEmissive =
            entry.path().stem().string().find("light") != std::string::npos;
        if (looksEmissive) {
            m.material = std::string("EMIT");
            m.emission = Vector3f(40.0f);
        }
        d.meshes.push_back(std::move(m));
    }
}

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

Vector3f toVector3f(const objl::Vector3& v)
{
    return {v.X, v.Y, v.Z};
}

bool nearZero(const Vector3f& v)
{
    return std::fabs(v.x) < EPSILON &&
           std::fabs(v.y) < EPSILON &&
           std::fabs(v.z) < EPSILON;
}

float maxComponent(const Vector3f& v)
{
    return std::max(v.x, std::max(v.y, v.z));
}

bool nearlyOpaque(const std::optional<objl::Material>& src)
{
    return !src || src->d >= 0.999f;
}

std::string inferMaterialType(const std::optional<objl::Material>& src)
{
    if (!src) return "DIFFUSE";
    std::string name = lowercase(src->name);
    Vector3f ks = toVector3f(src->Ks);
    float ksMax = maxComponent(ks);

    if (!nearlyOpaque(src) || !src->map_d.empty() ||
        src->illum == 4 || src->illum == 6 || src->illum == 7 || src->illum == 9 ||
        name.find("glass") != std::string::npos) {
        return "GLASS";
    }

    if (name.find("mirror") != std::string::npos ||
        ((name.find("chrome") != std::string::npos || name.find("polished") != std::string::npos) &&
         ksMax > 0.8f && nearZero(toVector3f(src->Kd)))) {
        return "MIRROR";
    }

    if (name.find("metal")  != std::string::npos ||
        name.find("chrome") != std::string::npos ||
        name.find("steel")  != std::string::npos ||
        name.find("brass")  != std::string::npos ||
        name.find("silver") != std::string::npos) return "METAL";

    if (name.find("plastic") != std::string::npos ||
        name.find("rubber")  != std::string::npos ||
        name.find("paper")   != std::string::npos ||
        name.find("label")   != std::string::npos ||
        name.find("tape")    != std::string::npos) return "MICROFACET";

    if (ksMax > 0.7f && src->Ns > 100.0f) return "METAL";
    if (ksMax > 0.1f || !src->map_Ks.empty() || !src->map_Ns.empty() || !src->map_bump.empty()) {
        return "MICROFACET";
    }

    return "DIFFUSE";
}

Vector3f inferColor(const std::optional<objl::Material>& src)
{
    if (!src) return Vector3f(0.8f);
    Vector3f kd = toVector3f(src->Kd);
    return nearZero(kd) ? Vector3f(1.0f) : kd;
}

float inferIor(const std::optional<objl::Material>& src)
{
    return (src && src->Ni > EPSILON) ? src->Ni : 1.5f;
}

float inferRoughness(const std::optional<objl::Material>& src)
{
    if (!src) return 0.5f;
    float ns = std::max(0.0f, src->Ns);
    // Map MTL shininess (large = glossy) onto our roughness knob.
    float rough = std::clamp(std::sqrt(2.0f / (ns + 2.0f)), 0.05f, 1.0f);
    if (!src->map_bump.empty()) rough = std::min(1.0f, rough * 1.15f);
    return rough;
}

Vector3f inferEmission(const std::optional<objl::Material>& src)
{
    (void)src;
    return Vector3f(0.0f);
}

MaterialMapConfig loadMaterialMap(const fs::path& path)
{
    MaterialMapConfig cfg;
    if (path.empty()) return cfg;

    cfg.sourceDir = path.parent_path();
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open materialMap file: " + path.string());
    }

    MaterialMapParser parser(tokenize(in), path.string());
    parser.parse(cfg);
    return cfg;
}

} // namespace

// ===== Public entry point ================================================

LoadedScene SceneLoader::load(const std::string& sceneDir,
                              const std::string& viewName,
                              const LoadOverrides& overrides)
{
    LoadedScene out;
    fs::path sceneRoot(sceneDir);
    fs::path sceneFile = sceneRoot / (viewName + ".scene");

    SceneDescriptor desc;
    bool parsed = false;
    if (fs::exists(sceneFile)) {
        std::ifstream in(sceneFile);
        if (!in) {
            std::cerr << "SceneLoader: failed to open " << sceneFile << "\n";
        } else {
            try {
                Parser parser(tokenize(in), sceneFile.string());
                parser.parse(desc);
                parsed = true;
                std::cerr << "SceneLoader: parsed " << sceneFile
                          << " (" << desc.meshes.size() << " meshes, "
                          << desc.lights.size() << " lights)\n";
            } catch (const std::exception& e) {
                std::cerr << "SceneLoader: parse error — " << e.what() << "\n"
                          << "SceneLoader: falling back to directory scan.\n";
            }
        }
    }
    if (!parsed) {
        std::cerr << "SceneLoader: no usable " << sceneFile
                  << "; scanning " << sceneRoot
                  << " for .obj files with default materials.\n";
        fallbackDirectoryScan(sceneRoot, desc);
    }

    applyOverrides(desc, overrides);

    // ---- build scene -------------------------------------------------------
    auto scene = std::make_unique<Scene>(desc.width, desc.height);
    scene->spp             = desc.spp;
    scene->maxDepth        = desc.maxDepth;
    scene->shadowsEnabled  = desc.shadowsEnabled;
    scene->adaptiveSamplingEnabled = desc.adaptiveSamplingEnabled;
    scene->adaptiveMinSpp  = std::max(1, desc.adaptiveMinSpp);
    scene->adaptiveBatchSize = std::max(1, desc.adaptiveBatchSize);
    scene->adaptiveThreshold = std::max(0.0f, desc.adaptiveThreshold);
    scene->denoiseEnabled  = desc.denoiseEnabled;
    scene->denoiseRadius   = std::max(0, desc.denoiseRadius);
    scene->denoiseSigmaSpatial = std::max(0.01f, desc.denoiseSigmaSpatial);
    scene->denoiseSigmaColor = std::max(0.001f, desc.denoiseSigmaColor);
    scene->denoiseSigmaNormal = std::max(0.001f, desc.denoiseSigmaNormal);
    scene->denoiseSigmaAlbedo = std::max(0.001f, desc.denoiseSigmaAlbedo);
    scene->backgroundColor = desc.background;
    scene->outputBaseName  = "output/" + viewName;

    std::unordered_map<std::string, Texture*> textureCache;
    auto loadTextureCached = [&](const fs::path& texPath) -> Texture* {
        if (texPath.empty()) return nullptr;
        std::string key = texPath.lexically_normal().string();
        auto it = textureCache.find(key);
        if (it != textureCache.end()) return it->second;

        auto tex = std::make_unique<Texture>();
        if (!tex->load(key)) return nullptr;

        Texture* ptr = tex.get();
        textureCache[key] = ptr;
        out.textures.push_back(std::move(tex));
        return ptr;
    };

    // ---- materials + meshes ------------------------------------------------
    for (auto& meshSpec : desc.meshes) {
        fs::path objPath = resolveRelative(sceneRoot, meshSpec.objPath);
        if (!fs::exists(objPath)) {
            std::cerr << "SceneLoader: WARN " << objPath
                      << " not found, skipping.\n";
            continue;
        }

        objl::Loader loader;
        if (!loader.LoadFile(objPath.generic_string())) {
            std::cerr << "SceneLoader: WARN failed to load OBJ " << objPath << "\n";
            continue;
        }

        std::vector<Material*> submeshMaterials;
        submeshMaterials.reserve(loader.LoadedMeshes.size());
        fs::path objDir = objPath.parent_path();
        MaterialMapConfig materialMap;
        if (meshSpec.materialMap) {
            fs::path materialMapPath = resolveRelative(sceneRoot, *meshSpec.materialMap);
            try {
                materialMap = loadMaterialMap(materialMapPath);
            } catch (const std::exception& e) {
                std::cerr << "SceneLoader: WARN " << e.what() << "\n";
            }
        }
        std::unordered_set<std::string> matchedOverrides;

        for (const auto& loadedMesh : loader.LoadedMeshes) {
            std::optional<objl::Material> srcMaterial = loadedMesh.MeshMaterial;
            const MaterialOverrideSpec* override = nullptr;
            if (srcMaterial) {
                auto it = materialMap.overrides.find(srcMaterial->name);
                if (it != materialMap.overrides.end()) {
                    override = &it->second;
                    matchedOverrides.insert(srcMaterial->name);
                }
            }

            std::string materialType = MaterialFactory::normalizeTypeName(meshSpec.material.value_or(
                override && override->type ? *override->type :
                materialMap.defaultType.value_or(inferMaterialType(srcMaterial))));
            Vector3f color = meshSpec.color.value_or(
                override && override->color ? *override->color : inferColor(srcMaterial));
            MaterialConfig config;
            config.color = color;
            config.ior = meshSpec.ior.value_or(
                override && override->ior ? *override->ior : inferIor(srcMaterial));
            config.metallic = meshSpec.metallic.value_or(
                override && override->metallic ? *override->metallic : 0.0f);
            config.roughness = meshSpec.roughness.value_or(
                override && override->roughness ? *override->roughness : inferRoughness(srcMaterial));
            if (!meshSpec.roughness && !(override && override->roughness) &&
                materialMap.defaultRoughnessMin) {
                config.roughness = std::max(config.roughness, *materialMap.defaultRoughnessMin);
            }
            config.shadowTransmission = meshSpec.shadowTransmission.value_or(
                override && override->shadowTransmission
                    ? *override->shadowTransmission
                    : (materialType == "GLASS" ? 0.85f : 0.0f));
            config.shadowTint = meshSpec.shadowTint.value_or(
                override && override->shadowTint ? *override->shadowTint : Vector3f(1.0f));
            if (srcMaterial) {
                config.Kd = clamp(0.0f, 1.0f, maxComponent(toVector3f(srcMaterial->Kd)));
                config.Ks = clamp(0.0f, 1.0f, maxComponent(toVector3f(srcMaterial->Ks)));
                config.specularExponent = srcMaterial->Ns;
            }
            if (override && override->Kd) config.Kd = clamp(0.0f, 1.0f, *override->Kd);
            if (override && override->Ks) config.Ks = clamp(0.0f, 1.0f, *override->Ks);
            if (materialType == "METAL" && !(override && override->Ks) && config.Ks < 0.5f) config.Ks = 0.9f;
            if (materialType == "MICROFACET" && !(override && override->Ks) && config.Ks < 0.15f) config.Ks = 0.25f;

            Vector3f emission = meshSpec.emission.value_or(
                override && override->emission ? *override->emission : inferEmission(srcMaterial));
            if (materialType == "EMIT" || emission.norm() > 0.0f) {
                config.emission = emission;
            }

            if (meshSpec.diffuseTex) {
                config.diffuseTexPath = resolveRelative(sceneRoot, *meshSpec.diffuseTex).string();
            } else if (override && override->diffuseTex) {
                config.diffuseTexPath = resolveRelative(materialMap.sourceDir, *override->diffuseTex).string();
            } else if (srcMaterial && !srcMaterial->map_Kd.empty()) {
                config.diffuseTexPath = resolveRelative(objDir, srcMaterial->map_Kd).string();
            }
            if (config.Kd < EPSILON && !config.diffuseTexPath.empty()) {
                config.Kd = 1.0f;
            }

            if (meshSpec.metallicTex) {
                config.metallicTexPath = resolveRelative(sceneRoot, *meshSpec.metallicTex).string();
            } else if (override && override->metallicTex) {
                config.metallicTexPath = resolveRelative(materialMap.sourceDir, *override->metallicTex).string();
            }
            if (!config.metallicTexPath.empty() && config.metallic <= EPSILON) {
                config.metallic = 1.0f;
            }

            if (meshSpec.normalTex) {
                config.normalTexPath = resolveRelative(sceneRoot, *meshSpec.normalTex).string();
            } else if (override && override->normalTex) {
                config.normalTexPath = resolveRelative(materialMap.sourceDir, *override->normalTex).string();
            }

            if (meshSpec.bumpTex) {
                config.bumpTexPath = resolveRelative(sceneRoot, *meshSpec.bumpTex).string();
            } else if (override && override->bumpTex) {
                config.bumpTexPath = resolveRelative(materialMap.sourceDir, *override->bumpTex).string();
            } else if (srcMaterial && !srcMaterial->map_bump.empty()) {
                config.bumpTexPath = resolveRelative(objDir, srcMaterial->map_bump).string();
            }

            if (meshSpec.bumpScale) {
                config.bumpScale = *meshSpec.bumpScale;
            } else if (override && override->bumpScale) {
                config.bumpScale = *override->bumpScale;
            } else if (!config.bumpTexPath.empty()) {
                config.bumpScale = 0.075f;
            }

            if (meshSpec.roughnessTex) {
                config.roughnessTexPath = resolveRelative(sceneRoot, *meshSpec.roughnessTex).string();
                config.roughnessTexIsGloss = meshSpec.roughnessTexIsGloss;
            } else if (override && override->roughnessTex) {
                config.roughnessTexPath = resolveRelative(materialMap.sourceDir, *override->roughnessTex).string();
                config.roughnessTexIsGloss = override->roughnessTexIsGloss.value_or(false);
            } else if (srcMaterial && !srcMaterial->map_Ns.empty()) {
                config.roughnessTexPath = resolveRelative(objDir, srcMaterial->map_Ns).string();
                config.roughnessTexIsGloss = true;
            }

            if (!config.diffuseTexPath.empty()) {
                if (Texture* tex = loadTextureCached(config.diffuseTexPath)) {
                    config.diffuseTexture = tex;
                    config.textured = true;
                } else {
                    std::cerr << "SceneLoader: WARN failed to load diffuse texture \""
                              << config.diffuseTexPath << "\" for " << meshSpec.objPath << "\n";
                }
            }

            if (!config.metallicTexPath.empty()) {
                if (Texture* tex = loadTextureCached(config.metallicTexPath)) {
                    config.metallicTexture = tex;
                } else {
                    std::cerr << "SceneLoader: WARN failed to load metallic texture \""
                              << config.metallicTexPath << "\" for " << meshSpec.objPath << "\n";
                }
            }

            if (!config.normalTexPath.empty()) {
                if (Texture* tex = loadTextureCached(config.normalTexPath)) {
                    config.normalTexture = tex;
                } else {
                    std::cerr << "SceneLoader: WARN failed to load normal texture \""
                              << config.normalTexPath << "\" for " << meshSpec.objPath << "\n";
                }
            }

            if (!config.bumpTexPath.empty()) {
                if (Texture* tex = loadTextureCached(config.bumpTexPath)) {
                    config.bumpTexture = tex;
                } else {
                    std::cerr << "SceneLoader: WARN failed to load bump texture \""
                              << config.bumpTexPath << "\" for " << meshSpec.objPath << "\n";
                }
            }

            if (!config.roughnessTexPath.empty()) {
                if (Texture* tex = loadTextureCached(config.roughnessTexPath)) {
                    config.roughnessTexture = tex;
                } else {
                    std::cerr << "SceneLoader: WARN failed to load roughness texture \""
                              << config.roughnessTexPath << "\" for " << meshSpec.objPath << "\n";
                }
            }

            auto mat = MaterialFactory::instance().create(materialType, config);
            submeshMaterials.push_back(mat.get());
            out.materials.push_back(std::move(mat));
        }

        for (const auto& [materialName, unused] : materialMap.overrides) {
            (void)unused;
            if (matchedOverrides.find(materialName) == matchedOverrides.end()) {
                std::cerr << "SceneLoader: WARN materialMap entry \"" << materialName
                          << "\" did not match any OBJ submaterial in " << meshSpec.objPath << "\n";
            }
        }

        auto mesh = std::make_unique<MeshTriangle>(loader,
                                                   meshSpec.offset,
                                                   meshSpec.rotation,
                                                   meshSpec.scale,
                                                   submeshMaterials);
        scene->Add(mesh.get());
        if (mesh->hasEmit()) {
            scene->AddLight(std::make_unique<MeshLight>(mesh.get()));
        }
        out.objects.push_back(std::move(mesh));
    }

    // ---- lights ------------------------------------------------------------
    for (auto& L : desc.lights) {
        if (!L.enabled) continue;
        if (isAreaLightType(L.type)) {
            switch (L.type) {
            case LightType::AreaRect:
                scene->AddLight(std::make_unique<AreaRectLight>(L.position, L.direction,
                                                                L.size, L.emission,
                                                                L.intensity, L.enabled));
                break;
            case LightType::AreaDisk:
                scene->AddLight(std::make_unique<AreaDiskLight>(L.position, L.direction,
                                                                L.radius, L.emission,
                                                                L.intensity, L.enabled));
                break;
            case LightType::AreaSphere:
                scene->AddLight(std::make_unique<AreaSphereLight>(L.position, L.radius,
                                                                  L.emission, L.intensity,
                                                                  L.enabled));
                break;
            case LightType::Point:
            case LightType::Spot:
            case LightType::Directional:
            case LightType::MeshEmit:
                break;
            }

            if (!L.showModel) continue;

            MaterialConfig lightMaterialConfig;
            lightMaterialConfig.color = Vector3f(1.0f);
            lightMaterialConfig.emission = L.emission * L.intensity;
            auto mat = MaterialFactory::instance().create("EMIT", lightMaterialConfig);

            if (L.type == LightType::AreaSphere) {
                auto sphere = std::make_unique<Sphere>(L.position, L.radius, mat.get());
                scene->Add(sphere.get());
                out.objects.push_back(std::move(sphere));
                out.materials.push_back(std::move(mat));
                continue;
            }

            Vector3f normal = lightNormalFromDirection(L.direction);
            Vector3f tangent;
            Vector3f bitangent;
            buildLightFrame(normal, tangent, bitangent);

            std::vector<Vector3f> verts;
            std::vector<uint32_t> idx;
            std::vector<Vector2f> st;

            if (L.type == LightType::AreaRect) {
                float sx = L.size.x * 0.5f;
                float sz = L.size.y * 0.5f;
                verts = {
                    L.position - tangent * sx - bitangent * sz,
                    L.position + tangent * sx - bitangent * sz,
                    L.position + tangent * sx + bitangent * sz,
                    L.position - tangent * sx + bitangent * sz,
                };
                idx = {0, 1, 2, 0, 2, 3};
                st = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            } else if (L.type == LightType::AreaDisk) {
                constexpr int kSegments = 32;
                verts.reserve(kSegments + 1);
                st.reserve(kSegments + 1);
                idx.reserve(kSegments * 3);
                verts.push_back(L.position);
                st.emplace_back(0.5f, 0.5f);
                for (int i = 0; i < kSegments; ++i) {
                    float phi = 2.0f * static_cast<float>(M_PI) *
                                static_cast<float>(i) / static_cast<float>(kSegments);
                    float c = std::cos(phi);
                    float s = std::sin(phi);
                    verts.push_back(L.position +
                                    tangent * (c * L.radius) +
                                    bitangent * (s * L.radius));
                    st.emplace_back(0.5f + 0.5f * c, 0.5f + 0.5f * s);
                }
                for (int i = 0; i < kSegments; ++i) {
                    int i0 = 0;
                    int i1 = 1 + i;
                    int i2 = 1 + ((i + 1) % kSegments);
                    idx.push_back(static_cast<uint32_t>(i0));
                    idx.push_back(static_cast<uint32_t>(i1));
                    idx.push_back(static_cast<uint32_t>(i2));
                }
            }

            auto mesh = std::make_unique<MeshTriangle>(verts.data(), idx.data(),
                                                       static_cast<uint32_t>(idx.size() / 3),
                                                       st.data(), mat.get());
            scene->Add(mesh.get());
            out.objects.push_back(std::move(mesh));
            out.materials.push_back(std::move(mat));
            continue;
        }

        switch (L.type) {
        case LightType::Point:
            scene->AddLight(std::make_unique<PointLight>(L.position, L.emission,
                                                         L.intensity, L.range, L.enabled));
            break;
        case LightType::Spot:
            scene->AddLight(std::make_unique<SpotLight>(L.position, L.direction, L.emission,
                                                        L.intensity, L.innerAngleDeg,
                                                        L.outerAngleDeg, L.range, L.enabled));
            break;
        case LightType::Directional:
            scene->AddLight(std::make_unique<DirectionalLight>(L.direction, L.emission,
                                                               L.intensity, L.enabled));
            break;
        case LightType::AreaRect:
        case LightType::AreaDisk:
        case LightType::AreaSphere:
        case LightType::MeshEmit:
            break;
        }
    }

    // ---- camera ------------------------------------------------------------
    float aspect = static_cast<float>(desc.width) / static_cast<float>(desc.height);
    auto cam = std::make_unique<Camera>(
        desc.camera.position,
        desc.camera.lookAt,
        desc.camera.up,
        desc.camera.fovDeg,
        aspect,
        desc.camera.aperture,
        desc.camera.focusDist
    );
    scene->camera = cam.get();
    scene->buildBVH();
    scene->buildPrimaryRayAccel();

    // ---- environment map ---------------------------------------------------
    if (!desc.envmapPath.empty()) {
        auto env = std::make_unique<EnvironmentMap>();
        env->intensity = desc.envmapIntensity;
        env->rotationYDegrees = desc.envmapRotationY;
        bool ok = env->loadHDR(resolveRelative(sceneRoot, desc.envmapPath).string());
        if (!ok) {
            std::cerr << "SceneLoader: env-map \"" << desc.envmapPath
                      << "\" failed to load.\n";
        }
        scene->envmap = env.get();
        out.envmap    = std::move(env);
    }

    out.camera = std::move(cam);
    out.scene  = std::move(scene);
    return out;
}
