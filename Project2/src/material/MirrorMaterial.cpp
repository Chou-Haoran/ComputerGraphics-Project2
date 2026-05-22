#include "MaterialFactory.hpp"

namespace {

class MirrorMaterial final : public Material {
public:
    using Material::Material;

    const std::string& typeName() const override
    {
        static const std::string kType = "MIRROR";
        return kType;
    }

    MaterialInteraction resolveInteraction(const Vector3f& rayDir,
                                           const Vector3f& N) const override
    {
        MaterialInteraction interaction;
        interaction.kind = MaterialInteractionKind::Delta;
        interaction.bounceCount = 1;
        interaction.bounces[0].direction = normalize(reflect(rayDir, N));
        interaction.bounces[0].throughput = getColor();
        return interaction;
    }

    Vector3f sample(const Vector3f&, const Vector3f&,
                    const Vector3f&, float,
                    float) const override
    {
        return Vector3f(0.0f);
    }

    float pdf(const Vector3f&, const Vector3f&, const Vector3f&,
              const Vector3f&, float,
              float) const override
    {
        return 0.0f;
    }

    Vector3f eval(const Vector3f&, const Vector3f&, const Vector3f&,
                  const Vector3f&, float,
                  float) const override
    {
        return Vector3f(0.0f);
    }
};

const MaterialRegistrar<MirrorMaterial> kMirrorMaterialRegistrar("MIRROR");

} // namespace
