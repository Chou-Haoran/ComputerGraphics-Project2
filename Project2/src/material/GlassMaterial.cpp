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
                                           const Vector3f& N,
                                           int spectralChannel) const override
    {
        MaterialInteraction interaction;
        interaction.kind = MaterialInteractionKind::Delta;

        // R/G/B IOR offsets (B bends the most, like real glass / diamond).
        constexpr float kChannelOffsets[3] = { -1.0f, 0.0f, 1.0f };

        // Dispersive case: an un-tagged ray entering a dispersive surface.
        // Spawn one broadband reflect + three channel-locked refracts; each
        // refract's throughput is masked to its own RGB channel so the path
        // accumulates the correct per-wavelength radiance back at the camera.
        if (dispersion > 0.0f && spectralChannel < 0) {
            float krAvg = 0.0f;
            fresnel(rayDir, N, ior, krAvg);
            interaction.bounces[0].direction = normalize(reflect(rayDir, N));
            interaction.bounces[0].throughput = Vector3f(krAvg);
            interaction.bounces[0].channel    = -1;
            interaction.bounceCount = 1;

            const Vector3f channelMask[3] = {
                Vector3f(1.0f, 0.0f, 0.0f),
                Vector3f(0.0f, 1.0f, 0.0f),
                Vector3f(0.0f, 0.0f, 1.0f)
            };

            for (int c = 0; c < 3; ++c) {
                float iorC = ior + dispersion * kChannelOffsets[c];
                float krC = 0.0f;
                fresnel(rayDir, N, iorC, krC);
                Vector3f refractDir = refract(rayDir, N, iorC);
                if (refractDir.norm() < EPSILON) continue;  // TIR for this channel
                auto& bounce = interaction.bounces[interaction.bounceCount++];
                bounce.direction = normalize(refractDir);
                bounce.throughput = channelMask[c] * (1.0f - krC);
                bounce.channel = c;
            }
            return interaction;
        }

        // Non-dispersive (single IOR) or channel-locked path: classic
        // reflect / refract pair, but pick the channel's IOR when tagged
        // so the wavelength's bending continues consistently inside the gem.
        float effectiveIor = ior;
        if (spectralChannel >= 0 && dispersion > 0.0f) {
            effectiveIor = ior + dispersion * kChannelOffsets[spectralChannel];
        }

        float kr = 0.0f;
        fresnel(rayDir, N, effectiveIor, kr);

        interaction.bounces[0].direction = normalize(reflect(rayDir, N));
        interaction.bounces[0].throughput = Vector3f(kr);
        interaction.bounces[0].channel    = spectralChannel;
        interaction.bounceCount = 1;

        if (kr < 1.0f) {
            Vector3f refractDir = refract(rayDir, N, effectiveIor);
            if (refractDir.norm() >= EPSILON) {
                auto& bounce = interaction.bounces[interaction.bounceCount++];
                bounce.direction = normalize(refractDir);
                bounce.throughput = Vector3f(1.0f - kr);
                bounce.channel = spectralChannel;
            } else {
                interaction.bounces[0].throughput = Vector3f(1.0f);
            }
        }

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

const MaterialRegistrar<GlassMaterial> kGlassMaterialRegistrar("GLASS");

} // namespace
