#pragma once
#ifndef PROJECT2_PHOTON_MAP_H
#define PROJECT2_PHOTON_MAP_H

#include "Vector.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

// A photon deposited where a light-path-through-specular hit a diffuse-ish
// surface. Stored by the photon emission pass and looked up during the main
// render to estimate caustic radiance arriving at diffuse surfaces.
struct Photon {
    Vector3f position;
    Vector3f incoming;   // direction the photon was travelling when it hit (towards surface)
    Vector3f power;      // RGB flux carried by this photon, in radiometric units
};

// Hash-grid backed photon storage. We bucket photons by cell coordinates and
// only scan the 3x3x3 cells around a query point — gives near-O(1) lookups
// without dragging in a full kd-tree.
class PhotonMap {
public:
    void store(const Photon& p);
    void build(float cellSize);   // call once after all photons stored
    size_t size() const { return photons_.size(); }

    // Returns the cone-filtered flux density (Σ Φ_i · k_i / π r²) at the
    // query point. Multiply by the surface BRDF (already includes cos term
    // through the photon-flux formulation) to get caustic outgoing radiance.
    Vector3f estimateFluxDensity(const Vector3f& point,
                                 const Vector3f& normal,
                                 float radius,
                                 int maxPhotons = 400) const;

private:
    std::vector<Photon> photons_;
    float cellSize_ = 0.0f;
    std::unordered_map<int64_t, std::vector<int>> grid_;

    static int64_t hashCell(int i, int j, int k);
};

#endif  // PROJECT2_PHOTON_MAP_H
