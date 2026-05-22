#include "Material.hpp"

#include "MaterialFactory.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

Material::Material(const MaterialConfig& config)
    : m_color(config.color),
      m_emission(config.emission),
      ior(config.ior),
      metallic(config.metallic),
      Kd(config.Kd),
      Ks(config.Ks),
      specularExponent(config.specularExponent),
      textured(config.textured),
      diffuseTexture(config.diffuseTexture),
      metallicTexture(config.metallicTexture),
      normalTexture(config.normalTexture),
      bumpTexture(config.bumpTexture),
      roughnessTexture(config.roughnessTexture),
      diffuseTexPath(config.diffuseTexPath),
      metallicTexPath(config.metallicTexPath),
      normalTexPath(config.normalTexPath),
      bumpTexPath(config.bumpTexPath),
      roughnessTexPath(config.roughnessTexPath),
      roughnessTexIsGloss(config.roughnessTexIsGloss),
      bumpScale(config.bumpScale),
      roughness(config.roughness)
{
}

Vector3f Material::getColor() const { return m_color; }
Vector3f Material::getEmission() const { return m_emission; }

bool Material::hasEmission() const
{
    return dotProduct(m_emission, m_emission) > EPSILON;
}

MaterialInteraction Material::resolveInteraction(const Vector3f&, const Vector3f&) const
{
    MaterialInteraction interaction;
    if (hasEmission()) {
        interaction.kind = MaterialInteractionKind::Emission;
        interaction.emission = m_emission;
    }
    return interaction;
}

Vector3f Material::getColorAt(double u, double v, float lod) const
{
    if (textured && diffuseTexture && diffuseTexture->valid()) {
        Vector3f texel = diffuseTexture->sample(static_cast<float>(u), static_cast<float>(v), lod);
        return Vector3f(m_color.x * texel.x,
                        m_color.y * texel.y,
                        m_color.z * texel.z);
    }
    return m_color;
}

Vector3f Material::getColorAtAnisotropic(double u, double v,
                                         float footprintU, float footprintV,
                                         int maxSamples) const
{
    if (textured && diffuseTexture && diffuseTexture->valid()) {
        Vector3f texel = diffuseTexture->sampleAnisotropic(static_cast<float>(u),
                                                           static_cast<float>(v),
                                                           footprintU,
                                                           footprintV,
                                                           maxSamples);
        return Vector3f(m_color.x * texel.x,
                        m_color.y * texel.y,
                        m_color.z * texel.z);
    }
    return m_color;
}

float Material::getMetallicAt(double u, double v, float lod) const
{
    float out = metallic;
    if (metallicTexture && metallicTexture->valid()) {
        float tex = metallicTexture->sampleScalar(static_cast<float>(u), static_cast<float>(v), lod);
        out = metallic > EPSILON ? out * tex : tex;
    }
    return clamp(0.0f, 1.0f, out);
}

float Material::getRoughnessAt(double u, double v, float lod) const
{
    float out = roughness;
    if (roughnessTexture && roughnessTexture->valid()) {
        float tex = roughnessTexture->sampleScalar(static_cast<float>(u), static_cast<float>(v), lod);
        if (roughnessTexIsGloss) tex = 1.0f - tex;
        out *= tex;
    }
    return clamp(0.04f, 1.0f, out);
}

Vector3f Material::applyNormalMap(double u, double v,
                                  const Vector3f& N,
                                  const Vector3f& T,
                                  const Vector3f& B,
                                  float lod) const
{
    if ((!normalTexture || !normalTexture->valid()) &&
        (!bumpTexture || !bumpTexture->valid())) {
        return N;
    }

    Vector3f tangent = normalize(T - N * dotProduct(T, N));
    if (tangent.norm() < EPSILON) return N;

    Vector3f bitangent = normalize(B - N * dotProduct(B, N));
    if (bitangent.norm() < EPSILON) {
        bitangent = normalize(crossProduct(N, tangent));
    }

    Vector3f mapped(0.0f, 0.0f, 1.0f);
    if (normalTexture && normalTexture->valid()) {
        Vector3f texel = normalTexture->sample(static_cast<float>(u), static_cast<float>(v), lod);
        mapped = normalize(Vector3f(texel.x * 2.0f - 1.0f,
                                    texel.y * 2.0f - 1.0f,
                                    texel.z * 2.0f - 1.0f));
    }

    if (bumpTexture && bumpTexture->valid()) {
        float du = 1.0f / std::max(1, bumpTexture->width());
        float dv = 1.0f / std::max(1, bumpTexture->height());
        float hx1 = bumpTexture->sampleScalar(static_cast<float>(u + du), static_cast<float>(v));
        float hx0 = bumpTexture->sampleScalar(static_cast<float>(u - du), static_cast<float>(v));
        float hy1 = bumpTexture->sampleScalar(static_cast<float>(u), static_cast<float>(v + dv));
        float hy0 = bumpTexture->sampleScalar(static_cast<float>(u), static_cast<float>(v - dv));
        mapped.x -= (hx1 - hx0) * bumpScale;
        mapped.y -= (hy1 - hy0) * bumpScale;
        mapped = normalize(mapped);
    }

    return normalize(tangent * mapped.x + bitangent * mapped.y + N * mapped.z);
}

Vector3f Material::reflect(const Vector3f& I, const Vector3f& N) const
{
    return I - 2 * dotProduct(I, N) * N;
}

Vector3f Material::refract(const Vector3f& I, const Vector3f& N, const float& indexOfRefraction) const
{
    float cosi = clamp(-1, 1, dotProduct(I, N));
    float etai = 1.0f;
    float etat = indexOfRefraction;
    Vector3f n = N;
    if (cosi < 0) {
        cosi = -cosi;
    } else {
        std::swap(etai, etat);
        n = -N;
    }
    float eta = etai / etat;
    float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
    return k < 0.0f ? Vector3f(0) : eta * I + (eta * cosi - std::sqrt(k)) * n;
}

void Material::fresnel(const Vector3f& I, const Vector3f& N, const float& indexOfRefraction, float& kr) const
{
    float cosi = clamp(-1, 1, dotProduct(I, N));
    float etai = 1.0f;
    float etat = indexOfRefraction;
    if (cosi > 0.0f) {
        std::swap(etai, etat);
    }
    float sint = etai / etat * std::sqrt(std::max(0.0f, 1.0f - cosi * cosi));
    if (sint >= 1.0f) {
        kr = 1.0f;
    } else {
        float cost = std::sqrt(std::max(0.0f, 1.0f - sint * sint));
        cosi = std::fabs(cosi);
        float rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        kr = (rs * rs + rp * rp) / 2.0f;
    }
}

Vector3f Material::toWorld(const Vector3f& a, const Vector3f& N) const
{
    Vector3f B;
    Vector3f C;
    if (std::fabs(N.x) > std::fabs(N.y)) {
        float invLen = 1.0f / std::sqrt(N.x * N.x + N.z * N.z);
        C = Vector3f(N.z * invLen, 0.0f, -N.x * invLen);
    } else {
        float invLen = 1.0f / std::sqrt(N.y * N.y + N.z * N.z);
        C = Vector3f(0.0f, N.z * invLen, -N.y * invLen);
    }
    B = crossProduct(C, N);
    return a.x * B + a.y * C + a.z * N;
}

Vector3f Material::fresnelSchlick(float cosTheta, const Vector3f& F0) const
{
    float oneMinusCos = 1.0f - clamp(0.0f, 1.0f, cosTheta);
    float factor = oneMinusCos * oneMinusCos * oneMinusCos * oneMinusCos * oneMinusCos;
    return F0 + (Vector3f(1.0f) - F0) * factor;
}

Vector3f Material::blendF0(const Vector3f& albedo, float metallicValue) const
{
    const float t = clamp(0.0f, 1.0f, metallicValue);
    return Vector3f(0.04f) * (1.0f - t) + albedo * t;
}

float Material::luminance(const Vector3f& c) const
{
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

float Material::distributionGGX(const Vector3f& N, const Vector3f& H, float alpha) const
{
    float NdotH = std::max(0.0f, dotProduct(N, H));
    float a2 = alpha * alpha;
    float d = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / std::max(static_cast<float>(M_PI) * d * d, 1e-7f);
}

float Material::smithMaskingSchlickGGX(float NdotV, float alpha) const
{
    float k = (alpha + 1.0f) * (alpha + 1.0f) / 8.0f;
    return NdotV / std::max(NdotV * (1.0f - k) + k, 1e-7f);
}

float Material::geometrySmith(const Vector3f& N, const Vector3f& wi, const Vector3f& wo, float alpha) const
{
    float NdotV = std::max(0.0f, dotProduct(N, wi));
    float NdotL = std::max(0.0f, dotProduct(N, wo));
    return smithMaskingSchlickGGX(NdotV, alpha) * smithMaskingSchlickGGX(NdotL, alpha);
}

Vector3f Material::sampleCosineHemisphere(const Vector3f& N) const
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

float Material::pdfCosineHemisphere(const Vector3f& wo, const Vector3f& N) const
{
    return std::max(0.0f, dotProduct(wo, N)) / static_cast<float>(M_PI);
}

float Material::specularSampleWeight(const Vector3f& wi, const Vector3f& N,
                                     float rough, const Vector3f& F0,
                                     const Vector3f& diffuseColor) const
{
    float cosTheta = std::max(0.0f, dotProduct(wi, N));
    float fresnelLum = luminance(fresnelSchlick(cosTheta, F0));
    float diffuseWeight = std::max(0.0f, Kd) * std::max(0.02f, luminance(diffuseColor));
    float glossBoost = 0.35f + 0.65f * (1.0f - clamp(0.0f, 1.0f, rough));
    float specularWeight = std::max(0.0f, Ks) * std::max(0.08f, fresnelLum) * glossBoost * 4.0f;
    float sum = diffuseWeight + specularWeight;
    if (sum <= 1e-6f) return 0.5f;
    return clamp(0.1f, 0.9f, specularWeight / sum);
}

Vector3f Material::sampleGGXReflection(const Vector3f& wi, const Vector3f& N, float rough) const
{
    float alpha = std::max(0.04f, rough * rough);
    for (int attempt = 0; attempt < 4; ++attempt) {
        float u1 = get_random_float();
        float u2 = get_random_float();

        float phi = 2.0f * static_cast<float>(M_PI) * u1;
        float cosTheta = std::sqrt((1.0f - u2) /
                                   std::max(1.0f + (alpha * alpha - 1.0f) * u2, 1e-7f));
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
        Vector3f h = toWorld(Vector3f(std::cos(phi) * sinTheta,
                                      std::sin(phi) * sinTheta,
                                      cosTheta), N);
        Vector3f wo = reflect(-wi, normalize(h));
        if (dotProduct(wo, N) > 0.0f) {
            return normalize(wo);
        }
    }
    return sampleCosineHemisphere(N);
}

float Material::pdfGGX(const Vector3f& wi, const Vector3f& wo, const Vector3f& N, float rough) const
{
    if (dotProduct(wi, N) <= 0.0f || dotProduct(wo, N) <= 0.0f) return 0.0f;
    Vector3f h = normalize(wi + wo);
    if (h.norm() < EPSILON) return 0.0f;
    float alpha = std::max(0.04f, rough * rough);
    float D = distributionGGX(N, h, alpha);
    float NdotH = std::max(0.0f, dotProduct(N, h));
    float VdotH = std::max(EPSILON, dotProduct(wo, h));
    return D * NdotH / std::max(4.0f * VdotH, 1e-7f);
}
