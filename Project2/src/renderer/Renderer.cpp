#include "Renderer.hpp"
#include "Camera.hpp"
#include "Denoiser.hpp"
#include "ImageIO.hpp"
#include "Light.hpp"
#include "Material.hpp"
#include "PhotonMap.hpp"
#include "global.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef _OPENMP
    #include <omp.h>
#endif

namespace {

float luminance(const Vector3f& c)
{
    return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

Vector3f acesToneMap(const Vector3f& color)
{
    // Narkowicz ACES filmic fit — maps HDR linear radiance to [0,1] LDR
    // with a contrast-preserving S-curve that avoids highlight clipping.
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    auto curve = [&](float x) -> float {
        return (x * (a * x + b)) / (x * (c * x + d) + e);
    };

    return Vector3f(curve(color.x), curve(color.y), curve(color.z));
}

Vector3f primarySurfaceNormal(const Scene& scene, const Ray& ray, const Intersection& inter)
{
    Vector3f N = normalize(inter.normal);
    Vector3f viewDir = normalize(-ray.direction);
    if (inter.material->normalTexture && inter.material->normalTexture->valid()) {
        N = inter.material->applyNormalMap(inter.tcoords.x, inter.tcoords.y,
                                           N, inter.tangent, inter.bitangent);
        if (dotProduct(N, viewDir) < 0.0f) {
            N = -N;
        }
    }
    return N;
}

Denoiser::GuideBuffers buildGuideBuffers(const Scene& scene, const Camera& camera)
{
    Denoiser::GuideBuffers guides;
    const int pixelCount = scene.width * scene.height;
    guides.albedo.resize(pixelCount, Vector3f(0.0f));
    guides.normal.resize(pixelCount, Vector3f(0.0f));
    guides.valid.resize(pixelCount, 0);

    for (int j = 0; j < scene.height; ++j) {
        for (int i = 0; i < scene.width; ++i) {
            int pixelIdx = i + j * scene.width;
            float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(scene.width);
            float v = (static_cast<float>(j) + 0.5f) / static_cast<float>(scene.height);
            Ray ray = camera.generateRay(u, v, 0.5f, 0.5f);
            Intersection inter = scene.intersectPrimary(ray);
            if (!inter.happened || inter.material == nullptr || inter.obj == nullptr) {
                continue;
            }

            guides.valid[pixelIdx] = 1;
            guides.albedo[pixelIdx] = inter.obj->evalDiffuseColor(inter.tcoords);
            guides.normal[pixelIdx] = primarySurfaceNormal(scene, ray, inter);
        }
    }
    return guides;
}

Vector3f offsetOrigin(const Vector3f& point, const Vector3f& N, const Vector3f& dir)
{
    return dotProduct(dir, N) < 0.0f ? point - N * EPSILON
                                      : point + N * EPSILON;
}

// Trace one photon through specular bounces and store it where it lands on
// a non-delta surface. Caustic-only: we don't bounce off the diffuse hit
// because the photon map only models LS+D paths.
void tracePhoton(PhotonMap& map, const Scene& scene, Ray ray,
                 Vector3f power, int specularDepth, int maxDepth)
{
    while (maxDepth-- > 0) {
        Intersection hit = scene.intersect(ray);
        if (!hit.happened || !hit.material) return;

        Vector3f N = normalize(hit.normal);
        MaterialInteraction mi = hit.material->resolveInteraction(
            ray.direction, N, ray.spectralChannel);

        if (mi.kind == MaterialInteractionKind::Emission) return;  // absorbed

        if (mi.kind == MaterialInteractionKind::Delta) {
            // Stochastically pick one bounce by throughput strength so we
            // don't fan out exponentially (Russian-roulette style continuation).
            float weights[4];
            float total = 0.0f;
            for (int i = 0; i < mi.bounceCount; ++i) {
                const Vector3f& t = mi.bounces[i].throughput;
                weights[i] = std::max({t.x, t.y, t.z, 0.0f});
                total += weights[i];
            }
            if (total < 1e-6f) return;

            float r = get_random_float() * total;
            int chosen = mi.bounceCount - 1;
            float acc = 0.0f;
            for (int i = 0; i < mi.bounceCount; ++i) {
                acc += weights[i];
                if (r <= acc) { chosen = i; break; }
            }
            const MaterialDeltaBounce& b = mi.bounces[chosen];
            // Adjust power for the importance sampling of bounces.
            Vector3f stepThroughput = b.throughput * (total / std::max(weights[chosen], 1e-6f));
            power = Vector3f(power.x * stepThroughput.x,
                             power.y * stepThroughput.y,
                             power.z * stepThroughput.z);
            if (std::max({power.x, power.y, power.z}) < 1e-12f) return;

            Vector3f newOrigin = offsetOrigin(hit.coords, N, b.direction);
            ray = Ray(newOrigin, b.direction);
            ray.spectralChannel = b.channel >= 0 ? b.channel : ray.spectralChannel;
            ++specularDepth;
            continue;
        }

        // Non-delta hit: deposit photon if it travelled through specular at
        // least once, then stop (caustic-only photon map).
        if (specularDepth > 0) {
            Photon p;
            p.position = hit.coords;
            p.incoming = ray.direction;
            p.power = power;
            map.store(p);
        }
        return;
    }
}

struct SpecularTarget {
    Vector3f center;
    float    radius = 0.0f;
};

// Collect bounding spheres for every GLASS/MIRROR mesh so photon emission can
// importance-sample directions that actually have a chance to refract or
// reflect into a useful path.
std::vector<SpecularTarget> collectSpecularTargets(const Scene& scene)
{
    std::vector<SpecularTarget> out;
    for (const Object* obj : scene.get_objects()) {
        if (!obj) continue;
        const Material* mat = obj->getMaterial();
        if (!mat) continue;
        const std::string& name = mat->typeName();
        if (name != "GLASS" && name != "MIRROR") continue;
        Bounds3 b = const_cast<Object*>(obj)->getBounds();
        Vector3f center = (b.pMin + b.pMax) * 0.5f;
        Vector3f extent = (b.pMax - b.pMin) * 0.5f;
        float radius = extent.norm();   // bounding-sphere radius (over-estimates)
        if (radius <= 0.0f) continue;
        out.push_back({center, radius});
    }
    return out;
}

// Uniformly sample a direction inside the cone subtending `target` from
// `origin`. Returns the world-space direction and writes the solid-angle
// PDF to `outPdf`.
Vector3f sampleConeToward(const Vector3f& origin,
                          const SpecularTarget& target,
                          float& outPdf)
{
    Vector3f toCenter = target.center - origin;
    float d2 = dotProduct(toCenter, toCenter);
    float d = std::sqrt(d2);

    auto sampleUniformSphere = [](float& pdf) {
        pdf = 1.0f / (4.0f * static_cast<float>(M_PI));
        float u1 = get_random_float();
        float u2 = get_random_float();
        float z  = 1.0f - 2.0f * u1;
        float rr = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float phi = 2.0f * static_cast<float>(M_PI) * u2;
        return Vector3f(std::cos(phi) * rr, z, std::sin(phi) * rr);
    };

    if (d <= target.radius + EPSILON) {
        return sampleUniformSphere(outPdf);   // origin already inside target
    }

    Vector3f axis = toCenter / d;
    float sinAlphaMax = std::min(1.0f, target.radius / d);
    float cosAlphaMax = std::sqrt(std::max(0.0f, 1.0f - sinAlphaMax * sinAlphaMax));

    float u1 = get_random_float();
    float u2 = get_random_float();
    float cosAlpha = 1.0f - u1 * (1.0f - cosAlphaMax);
    float sinAlpha = std::sqrt(std::max(0.0f, 1.0f - cosAlpha * cosAlpha));
    float phi = 2.0f * static_cast<float>(M_PI) * u2;

    Vector3f local(std::cos(phi) * sinAlpha,
                   std::sin(phi) * sinAlpha,
                   cosAlpha);

    Vector3f tan = std::fabs(axis.y) > 0.999f
                       ? Vector3f(1.0f, 0.0f, 0.0f)
                       : normalize(crossProduct(Vector3f(0.0f, 1.0f, 0.0f), axis));
    Vector3f bit = normalize(crossProduct(axis, tan));

    outPdf = 1.0f / (2.0f * static_cast<float>(M_PI) * (1.0f - cosAlphaMax));
    return normalize(tan * local.x + bit * local.y + axis * local.z);
}

std::unique_ptr<PhotonMap> buildCausticPhotonMap(const Scene& scene,
                                                 int totalPhotons,
                                                 float gatherRadius)
{
    auto map = std::make_unique<PhotonMap>();
    if (totalPhotons <= 0 || scene.analyticLights.empty()) return map;

    float totalWeight = 0.0f;
    for (const auto& light : scene.analyticLights) {
        if (light && light->enabled()) totalWeight += light->samplingWeight();
    }
    if (totalWeight <= 0.0f) return map;

    const std::vector<SpecularTarget> targets = collectSpecularTargets(scene);
    const bool useImportance = !targets.empty();

    int emitted = 0;
    int rejected = 0;
    for (const auto& light : scene.analyticLights) {
        if (!light || !light->enabled()) continue;
        int nForLight = std::max(1, static_cast<int>(
            std::round(totalPhotons * light->samplingWeight() / totalWeight)));

        for (int i = 0; i < nForLight; ++i) {
            Vector3f origin, dir, power;

            if (useImportance) {
                // Emission-point sampling, then pick a specular target
                // uniformly and bias the direction into its subtended cone.
                Vector3f lightNormal;
                Vector3f radiance;
                float area = 0.0f;
                if (!light->sampleEmissionPoint(origin, lightNormal,
                                                radiance, area)) {
                    if (!light->samplePhoton(origin, dir, power, nForLight)) continue;
                } else {
                    const int targetIdx = static_cast<int>(
                        std::floor(get_random_float() *
                                   static_cast<float>(targets.size())));
                    const SpecularTarget& tgt = targets[std::min(
                        targetIdx, static_cast<int>(targets.size()) - 1)];
                    float conePdf = 0.0f;
                    dir = sampleConeToward(origin, tgt, conePdf);
                    // Direction may face away from the light's emitting hemisphere
                    // (Lambertian emitters only emit into +n half-space).
                    float cosTheta = dotProduct(dir, lightNormal);
                    if (cosTheta <= 0.0f || conePdf <= 0.0f) {
                        ++rejected;
                        continue;
                    }
                    // Per-photon flux for the chosen sampling distribution:
                    //   L · area · cosθ · selectionProb⁻¹ · pdf⁻¹ / N
                    float selectionInv = static_cast<float>(targets.size());
                    float scale = (area * cosTheta * selectionInv) /
                                  (conePdf * static_cast<float>(nForLight));
                    power = radiance * scale;
                }
            } else if (!light->samplePhoton(origin, dir, power, nForLight)) {
                continue;
            }

            tracePhoton(*map, scene, Ray(origin, dir), power, 0, 8);
        }
        emitted += nForLight;
    }

    map->build(2.0f * gatherRadius);
    std::cout << "Caustic photon pass: emitted " << emitted
              << ", rejected " << rejected
              << ", stored " << map->size()
              << " (importance-sampled toward " << targets.size()
              << " specular target" << (targets.size() == 1 ? "" : "s")
              << ")\n";
    return map;
}

} // namespace

void Renderer::Render(Scene& scene, const Camera& camera)
{
    std::vector<Vector3f> framebuffer(scene.width * scene.height);

    std::cout << "SPP: " << scene.spp << "\n";
    std::cout << "Adaptive sampling: "
              << (scene.adaptiveSamplingEnabled ? "on" : "off") << "\n";

    // ---- caustic photon emission ------------------------------------------
    std::unique_ptr<PhotonMap> photonMap;
    if (scene.causticPhotonCount > 0) {
        std::cout << "Caustic photons requested: " << scene.causticPhotonCount
                  << " (gather radius " << scene.causticGatherRadius << ")\n";
        photonMap = buildCausticPhotonMap(scene, scene.causticPhotonCount,
                                          scene.causticGatherRadius);
        scene.causticMap = photonMap.get();
    }

    constexpr float fireflyClamp = 1.5f;
    std::atomic<uint32_t> rowsDone{0};
    std::atomic<uint64_t> totalSamples{0};

#ifdef _OPENMP
    const int threadCount = std::max(1, omp_get_max_threads());
#else
    const int threadCount = 1;
#endif

#pragma omp parallel for schedule(dynamic, 1) num_threads(threadCount)
    for (int j = 0; j < scene.height; ++j) {
        for (int i = 0; i < scene.width; ++i) {
            int pixelIdx = i + j * scene.width;
            Vector3f color(0.0f);
            float luminanceMean = 0.0f;
            float luminanceM2 = 0.0f;
            int sampleCount = 0;

            while (sampleCount < scene.spp) {
                float u;
                float v;
                float r1;
                float r2;
                if (scene.spp == 1) {
                    u = (i + 0.5f) / scene.width;
                    v = (j + 0.5f) / scene.height;
                    r1 = 0.5f;
                    r2 = 0.5f;
                } else {
                    u  = (i + get_random_float()) / scene.width;
                    v  = (j + get_random_float()) / scene.height;
                    r1 = get_random_float();
                    r2 = get_random_float();
                }

                Ray r = camera.generateRay(u, v, r1, r2);
                int pathFlags = 0;
                Vector3f s = scene.castRay(r, 0, &pathFlags);
                // Per-sample radiance clamp to suppress specular-caustic
                // fireflies (rare diffuse→glossy→light paths). Skip the clamp
                // for samples that went through dispersive refraction so we
                // preserve the rainbow caustics that make a diamond sparkle.
                if (!(pathFlags & Scene::PathFlagTouchedDispersion)) {
                    s.x = std::min(s.x, fireflyClamp);
                    s.y = std::min(s.y, fireflyClamp);
                    s.z = std::min(s.z, fireflyClamp);
                }

                color += s;
                ++sampleCount;

                float l = luminance(s);
                float delta = l - luminanceMean;
                luminanceMean += delta / static_cast<float>(sampleCount);
                luminanceM2 += delta * (l - luminanceMean);

                bool adaptiveReady = scene.adaptiveSamplingEnabled &&
                                     scene.spp > 1 &&
                                     sampleCount >= std::min(scene.spp, scene.adaptiveMinSpp) &&
                                     (sampleCount % scene.adaptiveBatchSize == 0 ||
                                      sampleCount == scene.spp);
                if (!adaptiveReady) continue;

                float variance = sampleCount > 1 ? luminanceM2 /
                                 static_cast<float>(sampleCount - 1) : 0.0f;
                float confidenceInterval = 1.96f *
                                           std::sqrt(std::max(0.0f, variance) /
                                                     static_cast<float>(sampleCount));
                float relativeError = confidenceInterval /
                                      std::max(luminanceMean, scene.adaptiveLuminanceFloor);
                if (relativeError <= scene.adaptiveThreshold) {
                    break;
                }
            }

            framebuffer[pixelIdx] = color / static_cast<float>(std::max(1, sampleCount));
            totalSamples.fetch_add(static_cast<uint64_t>(sampleCount), std::memory_order_relaxed);
        }
        uint32_t finished = rowsDone.fetch_add(1, std::memory_order_relaxed) + 1;
#ifdef _OPENMP
#pragma omp critical(render_progress)
#endif
        UpdateProgress(static_cast<float>(finished) / static_cast<float>(scene.height));
    }
    UpdateProgress(1.0f);
    std::cout << std::endl;

    float avgSpp = static_cast<float>(totalSamples.load(std::memory_order_relaxed)) /
                   static_cast<float>(std::max(1, scene.width * scene.height));
    std::cout << "Average spp: " << avgSpp << "\n";

    if (scene.toneMap && scene.exposure > 0.0f) {
        std::cout << "Tone mapping: ACES filmic (exposure=" << scene.exposure << ")\n";
        for (auto& pixel : framebuffer) {
            pixel = acesToneMap(pixel * scene.exposure);
        }
    }

    const std::string ppmPath = scene.outputBaseName + ".ppm";
    const std::string pngPath = scene.outputBaseName + ".png";
    ImageIO::writePPM(ppmPath, framebuffer, scene.width, scene.height);
    ImageIO::writePNG(pngPath, framebuffer, scene.width, scene.height);

    if (scene.denoiseEnabled) {
        Denoiser::GuideBuffers guides = buildGuideBuffers(scene, camera);
        Denoiser::Settings settings;
        settings.radius = scene.denoiseRadius;
        settings.sigmaSpatial = scene.denoiseSigmaSpatial;
        settings.sigmaColor = scene.denoiseSigmaColor;
        settings.sigmaNormal = scene.denoiseSigmaNormal;
        settings.sigmaAlbedo = scene.denoiseSigmaAlbedo;

        std::vector<Vector3f> denoisedFramebuffer =
            Denoiser::jointBilateralFilter(framebuffer, guides,
                                           scene.width, scene.height, settings);
        const std::string denoisedPpmPath = scene.outputBaseName + "_denoised.ppm";
        const std::string denoisedPngPath = scene.outputBaseName + "_denoised.png";
        ImageIO::writePPM(denoisedPpmPath, denoisedFramebuffer, scene.width, scene.height);
        ImageIO::writePNG(denoisedPngPath, denoisedFramebuffer, scene.width, scene.height);
        std::cout << "Built-in denoised output written to "
                  << denoisedPngPath << "\n";
    }
}
