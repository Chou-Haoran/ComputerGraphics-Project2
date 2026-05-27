#ifndef PROJECT2_MATERIAL_H
#define PROJECT2_MATERIAL_H

#include "Texture.hpp"
#include "Vector.hpp"
#include "global.hpp"
#include <memory>
#include <string>

enum class MaterialInteractionKind {
    Surface,
    Emission,
    Delta
};

struct MaterialDeltaBounce {
    Vector3f direction  = Vector3f(0.0f);
    Vector3f throughput = Vector3f(0.0f);
    // -1 = inherit caller's channel; 0/1/2 = R/G/B (set by dispersive
    // refraction so the spawned child ray is locked to one wavelength).
    int      channel    = -1;
};

struct MaterialInteraction {
    MaterialInteractionKind kind = MaterialInteractionKind::Surface;
    Vector3f emission = Vector3f(0.0f);
    int bounceCount = 0;
    // Up to 1 reflect + 3 dispersive refracts (R/G/B).
    MaterialDeltaBounce bounces[4];
};

struct MaterialConfig {
    Vector3f color = Vector3f(0.0f);
    Vector3f emission = Vector3f(0.0f);
    float ior = 1.5f;
    // Per-channel IOR offset for GLASS dispersion: ior(R/G/B) = ior + offset*{-1,0,+1}.
    // 0 disables wavelength splitting and keeps refraction monochromatic.
    float dispersion = 0.0f;
    float metallic = 0.0f;
    float Kd = 0.8f;
    float Ks = 0.2f;
    float specularExponent = 25.0f;
    bool textured = false;
    const Texture* diffuseTexture = nullptr;
    const Texture* metallicTexture = nullptr;
    const Texture* normalTexture = nullptr;
    const Texture* bumpTexture = nullptr;
    const Texture* roughnessTexture = nullptr;
    const Texture* displacementTexture = nullptr;
    std::string diffuseTexPath;
    std::string metallicTexPath;
    std::string normalTexPath;
    std::string bumpTexPath;
    std::string roughnessTexPath;
    std::string displacementTexPath;
    bool roughnessTexIsGloss = false;
    float bumpScale = 0.05f;
    float roughness = 0.5f;
    float roughnessVariation = 0.0f;
    float displacementScale = 0.0f;
    float shadowTransmission = 0.0f;
    Vector3f shadowTint = Vector3f(1.0f);
};

class Material {
public:
    explicit Material(const MaterialConfig& config = {});
    virtual ~Material() = default;

    virtual const std::string& typeName() const = 0;
    virtual MaterialInteraction resolveInteraction(const Vector3f& rayDir,
                                                   const Vector3f& N,
                                                   int spectralChannel = -1) const;
    virtual Vector3f sample(const Vector3f& wi, const Vector3f& N,
                            const Vector3f& albedo, float rough,
                            float metallic) const = 0;
    virtual float    pdf(const Vector3f& wi, const Vector3f& wo,
                         const Vector3f& N, const Vector3f& albedo,
                         float rough, float metallic) const = 0;
    virtual Vector3f eval(const Vector3f& wi, const Vector3f& wo, const Vector3f& N,
                          const Vector3f& albedo, float rough, float metallic) const = 0;

    Vector3f getColor() const;
    Vector3f getColorAt(double u, double v, float lod = 0.0f) const;
    Vector3f getColorAtAnisotropic(double u, double v,
                                   float footprintU, float footprintV,
                                   int maxSamples = 8) const;
    float    getMetallicAt(double u, double v, float lod = 0.0f) const;
    float    getRoughnessAt(double u, double v, float lod = 0.0f) const;
    float    getDisplacementAt(double u, double v) const;
    Vector3f applyNormalMap(double u, double v,
                            const Vector3f& N,
                            const Vector3f& T,
                            const Vector3f& B,
                            float lod = 0.0f) const;
    Vector3f getShadowTransmittanceAt(double u, double v, float lod = 0.0f) const;
    Vector3f getEmission() const;
    bool hasEmission() const;

    Vector3f m_color;
    Vector3f m_emission;
    float ior;
    float dispersion;
    float metallic;
    float Kd;
    float Ks;
    float specularExponent;
    bool textured;
    const Texture* diffuseTexture;
    const Texture* metallicTexture;
    const Texture* normalTexture;
    const Texture* bumpTexture;
    const Texture* roughnessTexture;
    const Texture* displacementTexture;
    std::string diffuseTexPath;
    std::string metallicTexPath;
    std::string normalTexPath;
    std::string bumpTexPath;
    std::string roughnessTexPath;
    std::string displacementTexPath;
    bool roughnessTexIsGloss;
    float bumpScale;
    float roughness;
    float roughnessVariation;
    float displacementScale;
    float shadowTransmission;
    Vector3f shadowTint;

protected:
    Vector3f reflect(const Vector3f& I, const Vector3f& N) const;
    Vector3f refract(const Vector3f& I, const Vector3f& N, const float& ior) const;
    void fresnel(const Vector3f& I, const Vector3f& N, const float& ior, float& kr) const;
    Vector3f toWorld(const Vector3f& a, const Vector3f& N) const;
    Vector3f fresnelSchlick(float cosTheta, const Vector3f& F0) const;
    Vector3f blendF0(const Vector3f& albedo, float metallic) const;
    float luminance(const Vector3f& c) const;
    float distributionGGX(const Vector3f& N, const Vector3f& H, float alpha) const;
    float smithMaskingSchlickGGX(float NdotV, float alpha) const;
    float geometrySmith(const Vector3f& N, const Vector3f& wi, const Vector3f& wo, float alpha) const;
    Vector3f sampleCosineHemisphere(const Vector3f& N) const;
    float pdfCosineHemisphere(const Vector3f& wo, const Vector3f& N) const;
    float specularSampleWeight(const Vector3f& wi, const Vector3f& N,
                               float rough, const Vector3f& F0,
                               const Vector3f& diffuseColor) const;
    Vector3f sampleGGXReflection(const Vector3f& wi, const Vector3f& N, float rough) const;
    float pdfGGX(const Vector3f& wi, const Vector3f& wo, const Vector3f& N, float rough) const;
};

Material* getDefaultMaterial();

#endif // PROJECT2_MATERIAL_H
