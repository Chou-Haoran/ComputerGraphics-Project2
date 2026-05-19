#include "MaterialFactory.hpp"

namespace {

class EmitMaterial final : public Material {
public:
    using Material::Material;

    const std::string& typeName() const override
    {
        static const std::string kType = "EMIT";
        return kType;
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

const MaterialRegistrar<EmitMaterial> kEmitMaterialRegistrar("EMIT");

} // namespace
