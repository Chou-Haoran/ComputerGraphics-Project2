#include "MaterialFactory.hpp"

#include <algorithm>
#include <cmath>

namespace {

class DiffuseMaterial final : public Material {
public:
    using Material::Material;

    const std::string& typeName() const override
    {
        static const std::string kType = "DIFFUSE";
        return kType;
    }

    Vector3f sample(const Vector3f&, const Vector3f& N,
                    const Vector3f&, float,
                    float) const override
    {
        float x1 = get_random_float();
        float x2 = get_random_float();
        float r = std::sqrt(x1);
        float phi = 2.0f * static_cast<float>(M_PI) * x2;
        float x = r * std::cos(phi);
        float y = r * std::sin(phi);
        float z = std::sqrt(std::max(0.0f, 1.0f - x1));
        return toWorld(Vector3f(x, y, z), N);
    }

    float pdf(const Vector3f&, const Vector3f& wo, const Vector3f& N,
              const Vector3f&, float,
              float) const override
    {
        return std::max(0.0f, dotProduct(wo, N)) / static_cast<float>(M_PI);
    }

    Vector3f eval(const Vector3f& wi, const Vector3f& wo, const Vector3f& N,
                  const Vector3f& albedo, float,
                  float) const override
    {
        float NdotV = std::max(0.0f, dotProduct(N, wi));
        float NdotL = std::max(0.0f, dotProduct(N, wo));
        if (NdotV <= 0.0f || NdotL <= 0.0f) return Vector3f(0.0f);
        return albedo * (Kd / static_cast<float>(M_PI));
    }
};

const MaterialRegistrar<DiffuseMaterial> kDiffuseMaterialRegistrar("DIFFUSE");

} // namespace
