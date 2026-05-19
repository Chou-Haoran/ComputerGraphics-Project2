#include "MaterialFactory.hpp"

namespace {

class GlassMaterial final : public Material {
public:
    using Material::Material;

    const std::string& typeName() const override
    {
        static const std::string kType = "GLASS";
        return kType;
    }

    MaterialInteraction resolveInteraction(const Vector3f& rayDir,
                                           const Vector3f& N) const override
    {
        MaterialInteraction interaction;
        float kr = 0.0f;
        fresnel(rayDir, N, ior, kr);

        interaction.kind = MaterialInteractionKind::Delta;
        interaction.bounceCount = 1;
        interaction.bounces[0].direction = normalize(reflect(rayDir, N));
        interaction.bounces[0].throughput = Vector3f(kr);

        if (kr < 1.0f) {
            Vector3f refractDir = refract(rayDir, N, ior);
            if (refractDir.norm() >= EPSILON) {
                interaction.bounces[interaction.bounceCount].direction = normalize(refractDir);
                interaction.bounces[interaction.bounceCount].throughput = Vector3f(1.0f - kr);
                interaction.bounceCount++;
            } else {
                interaction.bounces[0].throughput = Vector3f(1.0f);
            }
        }

        return interaction;
    }

    Vector3f sample(const Vector3f&, const Vector3f&, float) const override
    {
        return Vector3f(0.0f);
    }

    float pdf(const Vector3f&, const Vector3f&, const Vector3f&, float) const override
    {
        return 0.0f;
    }

    Vector3f eval(const Vector3f&, const Vector3f&, const Vector3f&,
                  const Vector3f&, float) const override
    {
        return Vector3f(0.0f);
    }
};

const MaterialRegistrar<GlassMaterial> kGlassMaterialRegistrar("GLASS");

} // namespace
