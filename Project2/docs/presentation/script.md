# Presentation Script — Monte Carlo Path Tracer (main script ends at Slide 5)

> Team Collision: Jiandi Mu, Haoran Zhou, Ziyu Zhou, Kaiwei Wang.

> Main version: finish on Slide 5 to stay safely within the 4-minute limit.  
> Slide 6 is kept only as an optional backup if there is extra time.

---

## Slide 1 — Cover (~8 sec)

Good morning everyone. We are Team Collision. This is our Project 2 report, and our motivational image is a Vicorian style room, we will show that at the result part.

---

## Slide 2 — Rendering Pipeline Overview (~28 sec)

This slide shows our rendering pipeline. For each pixel, we generate camera rays, trace them into the scene, and use a BVH (Bounding Volume Hierarchy) for efficient intersection.

At each hit point, direct lighting uses MIS (Multiple Importance Sampling), combining light sampling and BSDF (Bidirectional Scattering Distribution Function) sampling. Indirect lighting comes from recursive path tracing, with Russian Roulette for termination.

---

## Slide 3 — Material System & GGX Microfacet (~25 sec)

All materials share one BSDF interface: `sample`, `eval`, and `pdf` (probability density function). We support six types of material: Diffuse, Microfacet, Metal, Mirror, Glass, and Emit.

Our main shading model is GGX (Trowbridge-Reitz) microfacet with the standard D, G, and F terms. Low roughness gives sharp highlights, while high roughness gives softer reflections.

---

## Slide 4 — Lighting & Environment Illumination (~22 sec)

For lighting, we support seven analytical light types, sampled by luminance.

We also support HDR (High Dynamic Range) environment maps with luminance-weighted sampling. Together with MIS, this reduces noise and gives more natural soft light in indoor scenes.

---

## Slide 5 — Results (~40 sec)

Here are our results.

The first scene is the desktop typewriter. The front view shows GGX highlights and soft contact shadows. The side view provides the second viewpoint with a desk. The lampshade shows translucent transmission and texture detail.

The second scene is the retro room. The Essential version shows indirect global illumination on the main furniture. The Grouped version extends this to the full room. The Denoised result reduces noise while preserving edges and shadows.

That concludes our main system and results. Thank you.

---

## Slide 6 — Optional Backup Only (~15 sec if time remains)

If there is still time, we can briefly mention three limitations: depth of field is not implemented yet, the BVH still uses naive median split rather than SAH (Surface Area Heuristic), and volumetric scattering is not supported. Future work would be thin-lens depth of field, SAH BVH optimisation, volumetric effects, and GPU acceleration.

---

## Appendix: Quick Q&A

- **Why NAIVE not SAH?** SAH adds complexity; NAIVE works well on our scenes; the SAH interface is stubbed out.
- **Why β=2?** Veach's thesis — β=2 minimises variance in practice.
- **Fireflies?** Per-sample clamp at 1.5 plus Russian Roulette on low-throughput paths.
- **Adaptive sampling?** Welford's online algorithm; stops when coefficient of variation drops below 0.08.
- **Custom .scene DSL (domain-specific language)?** glTF (GL Transmission Format) is heavy with dependencies; our DSL is ~400 lines, zero deps.
