#include "Scene.hpp"
#include "Camera.hpp"
#include "Material.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
struct TextureFootprint {
    float uvSpanU = 0.0f;
    float uvSpanV = 0.0f;
    float rhoU = 0.0f;
    float rhoV = 0.0f;
    float lod = 0.0f;
};

bool aabbInsidePlane(const Bounds3& bounds, const Vector3f& planeNormal, const Vector3f& planePoint)
{
    const Vector3f center = (bounds.pMin + bounds.pMax) * 0.5f;
    const Vector3f extent = (bounds.pMax - bounds.pMin) * 0.5f;
    const float radius = extent.x * std::fabs(planeNormal.x) +
                         extent.y * std::fabs(planeNormal.y) +
                         extent.z * std::fabs(planeNormal.z);
    const float signedDistance = dotProduct(planeNormal, center - planePoint);
    return signedDistance + radius >= 0.0f;
}

bool intersectsPrimaryFrustum(const Scene& scene, const Object* object)
{
    if (!scene.camera || !object) return true;

    const Bounds3 bounds = const_cast<Object*>(object)->getBounds();
    const Vector3f origin = scene.camera->position();
    const Vector3f forward = normalize(scene.camera->forwardDir());
    const Vector3f right = normalize(scene.camera->rightDir());
    const Vector3f up = normalize(scene.camera->upVector());

    const float tanHalfY = std::tan(0.5f * deg2rad(scene.camera->fov()));
    const float tanHalfX = tanHalfY * scene.camera->aspect();

    const Vector3f nearPoint = origin + forward * 0.02f;
    if (!aabbInsidePlane(bounds, forward, nearPoint)) return false;

    const Vector3f leftPlaneNormal = normalize(forward * tanHalfX + right);
    const Vector3f rightPlaneNormal = normalize(forward * tanHalfX - right);
    const Vector3f bottomPlaneNormal = normalize(forward * tanHalfY + up);
    const Vector3f topPlaneNormal = normalize(forward * tanHalfY - up);

    return aabbInsidePlane(bounds, leftPlaneNormal, origin) &&
           aabbInsidePlane(bounds, rightPlaneNormal, origin) &&
           aabbInsidePlane(bounds, bottomPlaneNormal, origin) &&
           aabbInsidePlane(bounds, topPlaneNormal, origin);
}

Vector3f offsetRayOrigin(const Vector3f& point, const Vector3f& normal, const Vector3f& dir)
{
    return dotProduct(dir, normal) < 0 ? point - normal * EPSILON
                                       : point + normal * EPSILON;
}

float powerHeuristic(float pdfA, float pdfB)
{
    float a2 = pdfA * pdfA;
    float b2 = pdfB * pdfB;
    return a2 / std::max(a2 + b2, 1e-8f);
}

float maxComponent(const Vector3f& v)
{
    return std::max(v.x, std::max(v.y, v.z));
}

Vector3f shadowTransmittance(const Scene& scene,
                             const Vector3f& point,
                             const Vector3f& normal,
                             const Vector3f& dir,
                             float maxDistance,
                             bool infiniteDistance)
{
    if (!scene.shadowsEnabled) return Vector3f(1.0f);

    Vector3f throughput(1.0f);
    Ray shadowRay(offsetRayOrigin(point, normal, dir), dir);
    float remaining = maxDistance;

    for (int depth = 0; depth < 8; ++depth) {
        Intersection shadowHit = scene.intersect(shadowRay);
        if (!shadowHit.happened) return throughput;

        if (!infiniteDistance &&
            shadowHit.tnear + 5.0f * EPSILON >= remaining) {
            return throughput;
        }

        if (!shadowHit.material) return Vector3f(0.0f);

        Vector3f pass = shadowHit.material->getShadowTransmittanceAt(
            shadowHit.tcoords.x, shadowHit.tcoords.y);
        if (maxComponent(pass) <= 1e-4f) {
            return Vector3f(0.0f);
        }

        throughput = throughput * pass;
        if (maxComponent(throughput) <= 1e-4f) {
            return Vector3f(0.0f);
        }

        if (!infiniteDistance) {
            remaining -= static_cast<float>(shadowHit.tnear);
            if (remaining <= EPSILON) return throughput;
        }

        shadowRay = Ray(offsetRayOrigin(shadowHit.coords,
                                        normalize(shadowHit.normal),
                                        dir),
                        dir);
    }

    return Vector3f(0.0f);
}

TextureFootprint estimateTextureFootprint(const Scene& scene,
                                          const Ray& ray,
                                          const Intersection& inter,
                                          const Texture* texture)
{
    TextureFootprint fp;
    if (!texture || !texture->valid() ||
        inter.dpdu.norm() < EPSILON || inter.dpdv.norm() < EPSILON ||
        scene.height <= 0) {
        return fp;
    }

    const float fovY = deg2rad(scene.camera ? scene.camera->fov()
                                            : static_cast<float>(scene.fov));
    const float hitDistance = std::max(static_cast<float>(inter.tnear), 1e-4f);
    const float pixelWorld = (hitDistance * std::tan(0.5f * fovY)) /
                             static_cast<float>(scene.height);
    const float ndotv = std::max(std::fabs(dotProduct(normalize(inter.normal),
                                                      normalize(-ray.direction))),
                                 0.35f);
    const float viewStretch = 1.0f / std::sqrt(ndotv);
    const float surfaceFootprint = pixelWorld * viewStretch;
    fp.uvSpanU = surfaceFootprint / std::max(inter.dpdu.norm(), 1e-6f);
    fp.uvSpanV = surfaceFootprint / std::max(inter.dpdv.norm(), 1e-6f);
    fp.uvSpanU = clamp(0.0f, 0.25f, fp.uvSpanU);
    fp.uvSpanV = clamp(0.0f, 0.25f, fp.uvSpanV);
    fp.rhoU = fp.uvSpanU * static_cast<float>(texture->width());
    fp.rhoV = fp.uvSpanV * static_cast<float>(texture->height());
    const float rho = std::max(fp.rhoU, fp.rhoV);

    if (rho > 1.0f) {
        fp.lod = clamp(0.0f, texture->maxLod(), std::log2(rho));
    }
    return fp;
}
}

void Scene::AddLight(std::unique_ptr<Light> light)
{
    if (!light) return;

    analyticLightWeightSum += std::max(light->samplingWeight(), 1e-4f);
    analyticLightCdf.push_back(analyticLightWeightSum);
    analyticLights.push_back(std::move(light));
}

void Scene::buildBVH() {
    if (!ENABLE_BVH) {
        bvh = nullptr;
        printf(" - BVH disabled, using brute-force intersections...\n\n");
        return;
    }
    printf(" - Generating BVH...\n\n");
    bvh = new BVHAccel(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

void Scene::buildPrimaryRayAccel()
{
    primaryRayObjects.clear();
    primaryBvh = nullptr;
    if (objects.empty()) return;

    for (auto* object : objects) {
        if (intersectsPrimaryFrustum(*this, object)) {
            primaryRayObjects.push_back(object);
        }
    }

    if (primaryRayObjects.empty()) {
        primaryRayObjects = objects;
    }

    printf(" - Primary-ray culling keeps %zu / %zu objects.\n",
           primaryRayObjects.size(), objects.size());

    if (!ENABLE_BVH || primaryRayObjects.size() <= 1) return;
    primaryBvh = new BVHAccel(primaryRayObjects, 1, BVHAccel::SplitMethod::NAIVE);
}

Intersection Scene::intersect(const Ray& ray) const
{
    if (ENABLE_BVH && bvh) return bvh->Intersect(ray);

    Intersection closest;
    for (auto* obj : objects) {
        Intersection hit = obj->getIntersection(ray);
        if (hit.happened && hit.tnear < closest.tnear) closest = hit;
    }
    return closest;
}

Intersection Scene::intersectPrimary(const Ray& ray) const
{
    if (primaryRayObjects.empty()) {
        return intersect(ray);
    }

    if (ENABLE_BVH && primaryBvh) return primaryBvh->Intersect(ray);

    Intersection closest;
    for (auto* obj : primaryRayObjects) {
        Intersection hit = obj->getIntersection(ray);
        if (hit.happened && hit.tnear < closest.tnear) closest = hit;
    }
    return closest;
}

void Scene::sampleLight(Intersection& pos, float& pdf) const
{
    pos = Intersection();
    pdf = 0.0f;
    float emit_area_sum = 0;
    for (auto* obj : objects) {
        if (obj->hasEmit()) {
            pos.happened = true;
            emit_area_sum += obj->getArea();
        }
    }
    float p = get_random_float() * emit_area_sum;
    emit_area_sum = 0;
    for (auto* obj : objects) {
        if (obj->hasEmit()) {
            emit_area_sum += obj->getArea();
            if (p <= emit_area_sum) {
                obj->Sample(pos, pdf);
                break;
            }
        }
    }
}

int Scene::sampleAnalyticLight(float u, float& pickPdf) const
{
    pickPdf = 0.0f;
    if (analyticLights.empty() || analyticLightWeightSum <= 0.0f) return -1;

    float target = clamp(0.0f, 1.0f - 1e-6f, u) * analyticLightWeightSum;
    auto it = std::upper_bound(analyticLightCdf.begin(), analyticLightCdf.end(), target);
    size_t index = static_cast<size_t>(std::distance(analyticLightCdf.begin(), it));
    if (index >= analyticLights.size()) {
        index = analyticLights.size() - 1;
    }

    float cdfPrev = index == 0 ? 0.0f : analyticLightCdf[index - 1];
    float weight = std::max(analyticLightCdf[index] - cdfPrev, 1e-4f);
    pickPdf = weight / analyticLightWeightSum;
    return static_cast<int>(index);
}

// Monte Carlo path tracer. Direct lighting combines sampled emissive geometry
// with analytic lights (point / spot / directional); indirect uses
// cosine-weighted hemisphere + Russian Roulette. Mirror & glass branch out
// with Fresnel-weighted reflection/refraction.
Vector3f Scene::castRay(const Ray& ray, int depth, int* pathFlags) const
{
    auto inter = depth == 0 ? intersectPrimary(ray) : intersect(ray);

    if (!inter.happened) {
        // PDF user-control (vi): environment map for the miss path.
        if (envmap && envmap->valid()) {
            return envmap->sample(normalize(ray.direction));
        }
        return backgroundColor;
    }

    Vector3f hitPoint   = inter.coords;
    Vector2f st         = inter.tcoords;
    Vector3f N          = normalize(inter.normal);
    Vector3f viewDir    = normalize(-ray.direction);
    const TextureFootprint diffuseFp =
        estimateTextureFootprint(*this, ray, inter, inter.material->diffuseTexture);
    const TextureFootprint metallicFp =
        estimateTextureFootprint(*this, ray, inter, inter.material->metallicTexture);
    const TextureFootprint normalFp =
        estimateTextureFootprint(*this, ray, inter, inter.material->normalTexture);
    const TextureFootprint roughnessFp =
        estimateTextureFootprint(*this, ray, inter, inter.material->roughnessTexture);
    Vector3f surfaceCol = inter.material->getColorAtAnisotropic(st.x, st.y,
                                                                diffuseFp.uvSpanU,
                                                                diffuseFp.uvSpanV);
    float surfaceMetallic = inter.material->getMetallicAt(st.x, st.y, metallicFp.lod);
    if (inter.material->normalTexture && inter.material->normalTexture->valid()) {
        N = inter.material->applyNormalMap(st.x, st.y, N, inter.tangent, inter.bitangent,
                                           normalFp.lod);
        if (dotProduct(N, viewDir) < 0.0f) N = -N;
    }
    float surfaceRoughness = inter.material->getRoughnessAt(st.x, st.y, roughnessFp.lod);
    MaterialInteraction interaction = inter.material->resolveInteraction(
        ray.direction, N, ray.spectralChannel);
    if (interaction.kind == MaterialInteractionKind::Emission) {
        return interaction.emission;
    }

    if (depth >= maxDepth) return Vector3f(0);

    if (interaction.kind == MaterialInteractionKind::Delta) {
        Vector3f radiance(0.0f);
        for (int i = 0; i < interaction.bounceCount; ++i) {
            const MaterialDeltaBounce& bounce = interaction.bounces[i];
            if (dotProduct(bounce.direction, bounce.direction) < EPSILON ||
                dotProduct(bounce.throughput, bounce.throughput) < EPSILON) continue;
            Ray childRay(offsetRayOrigin(hitPoint, N, bounce.direction),
                         bounce.direction);
            // Inherit the parent ray's channel unless this bounce was tagged
            // by a dispersive split, in which case the child is locked to a
            // wavelength so subsequent glass hits use that channel's IOR.
            childRay.spectralChannel = bounce.channel >= 0 ? bounce.channel
                                                            : ray.spectralChannel;
            if (pathFlags && bounce.channel >= 0 && ray.spectralChannel < 0) {
                *pathFlags |= PathFlagTouchedDispersion;
            }
            radiance += bounce.throughput * castRay(childRay, depth + 1, pathFlags);
        }
        return radiance;
    }

    Vector3f directLight(0), indirectLight(0);

    float lightPickPdf = 0.0f;
    int lightIndex = sampleAnalyticLight(get_random_float(), lightPickPdf);
    if (lightIndex >= 0 && lightPickPdf > 0.0f) {
        const auto& light = analyticLights[static_cast<size_t>(lightIndex)];
        LightSample sample;
        if (light && light->sampleLi(hitPoint, sample)) {
            float analyticCosTheta = std::max(0.0f, dotProduct(N, sample.direction));
            if (analyticCosTheta > 0.0f) {
                Vector3f visibility = shadowTransmittance(*this, hitPoint, N,
                                                          sample.direction,
                                                          sample.maxDistance,
                                                          sample.infiniteDistance);
                if (maxComponent(visibility) > 0.0f) {
                    Vector3f brdf = inter.material->eval(viewDir, sample.direction, N,
                                                         surfaceCol, surfaceRoughness,
                                                         surfaceMetallic);
                    float lightPdf = sample.pdf * lightPickPdf;
                    float weight = 1.0f;
                    if (!sample.delta) {
                        float bsdfPdf = inter.material->pdf(viewDir, sample.direction, N,
                                                            surfaceCol, surfaceRoughness,
                                                            surfaceMetallic);
                        weight = powerHeuristic(lightPdf, bsdfPdf);
                    }

                    directLight += sample.radiance * visibility * brdf * analyticCosTheta * weight /
                                   std::max(lightPdf, 1e-8f);
                }
            }
        }
    }

    if (envmap && envmap->valid()) {
        Vector3f envDir(0.0f);
        Vector3f envRadiance(0.0f);
        float envPdf = 0.0f;
        if (envmap->sampleDirection(envDir, envRadiance, envPdf)) {
            float envCosTheta = std::max(0.0f, dotProduct(N, envDir));
            if (envCosTheta > 0.0f && envPdf > 0.0f) {
                Vector3f visibility = shadowTransmittance(*this, hitPoint, N,
                                                          envDir,
                                                          std::numeric_limits<float>::infinity(),
                                                          true);
                if (maxComponent(visibility) > 0.0f) {
                    Vector3f brdf = inter.material->eval(viewDir, envDir, N,
                                                         surfaceCol, surfaceRoughness,
                                                         surfaceMetallic);
                    float bsdfPdf = inter.material->pdf(viewDir, envDir, N,
                                                        surfaceCol, surfaceRoughness,
                                                        surfaceMetallic);
                    float weight = powerHeuristic(envPdf, bsdfPdf);
                    directLight += envRadiance * visibility * brdf * envCosTheta * weight /
                                   std::max(envPdf, 1e-8f);
                }
            }
        }
    }

    if (get_random_float() < RussianRoulette) {
        Vector3f sampleDir = normalize(inter.material->sample(viewDir, N, surfaceCol,
                                                              surfaceRoughness,
                                                              surfaceMetallic));
        float    pdf       = inter.material->pdf(viewDir, sampleDir, N, surfaceCol,
                                                 surfaceRoughness,
                                                 surfaceMetallic);
        float    cosSample = std::max(0.0f, dotProduct(N, sampleDir));
        if (pdf > 1e-8f && cosSample > 0.0f) {
            Ray nextRay(offsetRayOrigin(hitPoint, N, sampleDir), sampleDir);
            nextRay.spectralChannel = ray.spectralChannel;
            Intersection nextInter = intersect(nextRay);
            if (!nextInter.happened) {
                Vector3f missRadiance(0.0f);
                if (envmap && envmap->valid()) {
                    missRadiance = envmap->sample(sampleDir);
                    float envPdf = envmap->pdf(sampleDir);
                    float weight = powerHeuristic(pdf, envPdf);
                    Vector3f brdf = inter.material->eval(viewDir, sampleDir, N,
                                                         surfaceCol, surfaceRoughness,
                                                         surfaceMetallic);
                    indirectLight = missRadiance * brdf * cosSample * weight /
                                    (pdf * RussianRoulette);
                } else {
                    Vector3f brdf = inter.material->eval(viewDir, sampleDir, N,
                                                         surfaceCol, surfaceRoughness,
                                                         surfaceMetallic);
                    indirectLight = castRay(nextRay, depth + 1, pathFlags) *
                                    brdf * cosSample / (pdf * RussianRoulette);
                }
            } else if (!nextInter.material->hasEmission()) {
                Vector3f brdf = inter.material->eval(viewDir, sampleDir, N,
                                                     surfaceCol, surfaceRoughness,
                                                     surfaceMetallic);
                indirectLight = castRay(nextRay, depth + 1, pathFlags) *
                                brdf * cosSample / (pdf * RussianRoulette);
            }
        }
    }
    return directLight + indirectLight;
}
