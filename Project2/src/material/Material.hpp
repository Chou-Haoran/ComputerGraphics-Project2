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
};

struct MaterialInteraction {
    MaterialInteractionKind kind = MaterialInteractionKind::Surface;
    Vector3f emission = Vector3f(0.0f);
    int bounceCount = 0;
    MaterialDeltaBounce bounces[2];
};

struct MaterialConfig {
    Vector3f color = Vector3f(0.0f);
    Vector3f emission = Vector3f(0.0f);
    float ior = 1.5f;
    float Kd = 0.8f;
    float Ks = 0.2f;
    float specularExponent = 25.0f;
    bool textured = false;
    const Texture* diffuseTexture = nullptr;
    const Texture* normalTexture = nullptr;
    const Texture* bumpTexture = nullptr;
    const Texture* roughnessTexture = nullptr;
    std::string diffuseTexPath;
    std::string normalTexPath;
    std::string bumpTexPath;
    std::string roughnessTexPath;
    bool roughnessTexIsGloss = false;
    float bumpScale = 0.05f;
    float roughness = 0.5f;
};

class Material {
public:
    explicit Material(const MaterialConfig& config = {});
    virtual ~Material() = default;

    virtual const std::string& typeName() const = 0;
    virtual MaterialInteraction resolveInteraction(const Vector3f& rayDir,
                                                   const Vector3f& N) const;
    virtual Vector3f sample(const Vector3f& wi, const Vector3f& N, float rough) const = 0;
    virtual float    pdf(const Vector3f& wi, const Vector3f& wo,
                         const Vector3f& N, float rough) const = 0;
    virtual Vector3f eval(const Vector3f& wi, const Vector3f& wo, const Vector3f& N,
                          const Vector3f& albedo, float rough) const = 0;

    Vector3f getColor() const;
    Vector3f getColorAt(double u, double v) const;
    float    getRoughnessAt(double u, double v) const;
    Vector3f applyNormalMap(double u, double v,
                            const Vector3f& N,
                            const Vector3f& T,
                            const Vector3f& B) const;
    Vector3f getEmission() const;
    bool hasEmission() const;

    Vector3f m_color;
    Vector3f m_emission;
    float ior;
    float Kd;
    float Ks;
    float specularExponent;
    bool textured;
    const Texture* diffuseTexture;
    const Texture* normalTexture;
    const Texture* bumpTexture;
    const Texture* roughnessTexture;
    std::string diffuseTexPath;
    std::string normalTexPath;
    std::string bumpTexPath;
    std::string roughnessTexPath;
    bool roughnessTexIsGloss;
    float bumpScale;
    float roughness;

protected:
    Vector3f reflect(const Vector3f& I, const Vector3f& N) const;
    Vector3f refract(const Vector3f& I, const Vector3f& N, const float& ior) const;
    void fresnel(const Vector3f& I, const Vector3f& N, const float& ior, float& kr) const;
    Vector3f toWorld(const Vector3f& a, const Vector3f& N) const;
    Vector3f fresnelSchlick(float cosTheta, const Vector3f& F0) const;
    float luminance(const Vector3f& c) const;
    float distributionGGX(const Vector3f& N, const Vector3f& H, float alpha) const;
    float smithMaskingSchlickGGX(float NdotV, float alpha) const;
    float geometrySmith(const Vector3f& N, const Vector3f& wi, const Vector3f& wo, float alpha) const;
    Vector3f sampleCosineHemisphere(const Vector3f& N) const;
    float pdfCosineHemisphere(const Vector3f& wo, const Vector3f& N) const;
    float specularSampleWeight(const Vector3f& wi, const Vector3f& N,
                               float rough, const Vector3f& F0) const;
    Vector3f sampleGGXReflection(const Vector3f& wi, const Vector3f& N, float rough) const;
    float pdfGGX(const Vector3f& wi, const Vector3f& wo, const Vector3f& N, float rough) const;
};

Material* getDefaultMaterial();

#endif // PROJECT2_MATERIAL_H
