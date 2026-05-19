#include "MaterialFactory.hpp"

#include <algorithm>
#include <cmath>

namespace {

class MicrofacetMaterial final : public Material {
public:
    using Material::Material;

    const std::string& typeName() const override
    {
        static const std::string kType = "MICROFACET";
        return kType;
    }

    Vector3f sample(const Vector3f& wi, const Vector3f& N, float rough) const override
    {
        float specularWeight = specularSampleWeight(wi, N, rough, Vector3f(0.04f));
        if (get_random_float() < specularWeight) {
            return sampleGGXReflection(wi, N, rough);
        }
        return sampleCosineHemisphere(N);
    }

    float pdf(const Vector3f& wi, const Vector3f& wo, const Vector3f& N, float rough) const override
    {
        if (dotProduct(wo, N) <= 0.0f) return 0.0f;
        float specularWeight = specularSampleWeight(wi, N, rough, Vector3f(0.04f));
        float diffusePdf = pdfCosineHemisphere(wo, N);
        return (1.0f - specularWeight) * diffusePdf + specularWeight * pdfGGX(wi, wo, N, rough);
    }

    Vector3f eval(const Vector3f& wi, const Vector3f& wo, const Vector3f& N,
                  const Vector3f& albedo, float rough) const override
    {
        float NdotV = std::max(0.0f, dotProduct(N, wi));
        float NdotL = std::max(0.0f, dotProduct(N, wo));
        if (NdotV <= 0.0f || NdotL <= 0.0f) return Vector3f(0.0f);

        Vector3f H = normalize(wi + wo);
        if (H.norm() < EPSILON) return Vector3f(0.0f);

        float alpha = std::max(0.04f, rough * rough);
        float D = distributionGGX(N, H, alpha);
        float G = geometrySmith(N, wi, wo, alpha);
        Vector3f F = fresnelSchlick(std::max(0.0f, dotProduct(H, wo)), Vector3f(0.04f));
        Vector3f spec = F * (Ks * D * G / std::max(4.0f * NdotV * NdotL, 1e-7f));
        Vector3f kd = Vector3f(1.0f - F.x, 1.0f - F.y, 1.0f - F.z);
        Vector3f diffuse = kd * albedo * (Kd / static_cast<float>(M_PI));
        return diffuse + spec;
    }
};

const MaterialRegistrar<MicrofacetMaterial> kMicrofacetMaterialRegistrar("MICROFACET");

} // namespace
