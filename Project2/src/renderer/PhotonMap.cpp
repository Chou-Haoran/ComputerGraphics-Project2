#include "PhotonMap.hpp"
#include "global.hpp"

#include <algorithm>
#include <cmath>

namespace {

// Cone filter from Jensen's "Realistic Image Synthesis Using Photon Mapping".
// k controls the falloff slope; the matching normalization (1 - 2/(3k)) keeps
// the estimator unbiased so flux density doesn't shrink as the cone narrows.
constexpr float kConeFilterK = 1.1f;

}  // namespace

void PhotonMap::store(const Photon& p) {
    photons_.push_back(p);
}

void PhotonMap::build(float cellSize) {
    cellSize_ = std::max(cellSize, 1e-6f);
    grid_.clear();
    for (int idx = 0; idx < static_cast<int>(photons_.size()); ++idx) {
        const Vector3f& p = photons_[idx].position;
        int i = static_cast<int>(std::floor(p.x / cellSize_));
        int j = static_cast<int>(std::floor(p.y / cellSize_));
        int k = static_cast<int>(std::floor(p.z / cellSize_));
        grid_[hashCell(i, j, k)].push_back(idx);
    }
}

Vector3f PhotonMap::estimateFluxDensity(const Vector3f& point,
                                        const Vector3f& normal,
                                        float radius,
                                        int /*maxPhotons*/) const {
    if (photons_.empty() || cellSize_ <= 0.0f) return Vector3f(0.0f);

    const float r2 = radius * radius;
    int i = static_cast<int>(std::floor(point.x / cellSize_));
    int j = static_cast<int>(std::floor(point.y / cellSize_));
    int k = static_cast<int>(std::floor(point.z / cellSize_));

    // Number of cell layers to scan: with cellSize == radius we only need ±1.
    // If the cell size differs from radius, dilate the neighbourhood enough
    // to cover the query disk.
    int reach = std::max(1, static_cast<int>(std::ceil(radius / cellSize_)));

    Vector3f flux(0.0f);
    for (int di = -reach; di <= reach; ++di) {
        for (int dj = -reach; dj <= reach; ++dj) {
            for (int dk = -reach; dk <= reach; ++dk) {
                auto it = grid_.find(hashCell(i + di, j + dj, k + dk));
                if (it == grid_.end()) continue;
                for (int idx : it->second) {
                    const Photon& ph = photons_[idx];
                    Vector3f d = ph.position - point;
                    float d2 = dotProduct(d, d);
                    if (d2 > r2) continue;
                    // Reject photons arriving from behind the receiving surface.
                    if (dotProduct(ph.incoming, normal) >= 0.0f) continue;
                    float dist = std::sqrt(d2);
                    float w = 1.0f - dist / (kConeFilterK * radius);
                    if (w < 0.0f) w = 0.0f;
                    flux = flux + ph.power * w;
                }
            }
        }
    }

    const float normalization =
        static_cast<float>(M_PI) * r2 * (1.0f - 2.0f / (3.0f * kConeFilterK));
    return flux / std::max(normalization, 1e-12f);
}

int64_t PhotonMap::hashCell(int i, int j, int k) {
    // Bit-packed signed 21-bit coords fit comfortably into 63 bits — much
    // cheaper than a real hash combine and good enough for our cell counts.
    constexpr int64_t mask = (int64_t(1) << 21) - 1;
    return ((int64_t(i) & mask) << 42) |
           ((int64_t(j) & mask) << 21) |
            (int64_t(k) & mask);
}
