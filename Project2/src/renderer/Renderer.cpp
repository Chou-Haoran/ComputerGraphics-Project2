#include "Renderer.hpp"
#include "Camera.hpp"
#include "Denoiser.hpp"
#include "ImageIO.hpp"
#include "global.hpp"
#include <atomic>
#include <cmath>
#include <iostream>
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
            Intersection inter = scene.intersect(ray);
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

} // namespace

void Renderer::Render(const Scene& scene, const Camera& camera)
{
    std::vector<Vector3f> framebuffer(scene.width * scene.height);

    std::cout << "SPP: " << scene.spp << "\n";
    std::cout << "Adaptive sampling: "
              << (scene.adaptiveSamplingEnabled ? "on" : "off") << "\n";

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
                Vector3f s = scene.castRay(r, 0);
                // Per-sample radiance clamp to suppress specular-caustic
                // fireflies (rare diffuse→glossy→light paths).
                s.x = std::min(s.x, fireflyClamp);
                s.y = std::min(s.y, fireflyClamp);
                s.z = std::min(s.z, fireflyClamp);

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
