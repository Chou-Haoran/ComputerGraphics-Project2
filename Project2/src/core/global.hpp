#pragma once
#include <iostream>
#include <cmath>
#include <random>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <algorithm>
#include <thread>

#undef M_PI
#define M_PI 3.141592653589793f

// Project2 only ships the production Monte Carlo path tracer, so the
// TASK_N / TASK_MAX_DEPTH / GLASS_IOR globals from Task-2 are dropped.
// EPSILON and ENABLE_BVH remain because they are read from multiple
// translation units (geometry / BVH / scene).
extern float EPSILON;
extern bool  ENABLE_BVH;

const float kInfinity = std::numeric_limits<float>::max();

inline float clamp(const float& lo, const float& hi, const float& v)
{
    return std::max(lo, std::min(hi, v));
}

inline float deg2rad(const float& deg) { return deg * M_PI / 180.0f; }

inline bool solveQuadratic(const float& a, const float& b, const float& c,
                           float& x0, float& x1)
{
    float discr = b * b - 4 * a * c;
    if (discr < 0) return false;
    else if (discr == 0) x0 = x1 = -0.5f * b / a;
    else {
        float q = (b > 0) ?
                  -0.5f * (b + std::sqrt(discr)) :
                  -0.5f * (b - std::sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }
    if (x0 > x1) std::swap(x0, x1);
    return true;
}

inline float get_random_float()
{
    thread_local std::mt19937 generator([] {
        std::random_device rd;
        const auto tidHash = static_cast<unsigned int>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()));
        return std::mt19937(rd() ^ (tidHash + 0x9e3779b9u));
    }());
    thread_local std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator);
}

inline void UpdateProgress(float progress)
{
    int barWidth = 70;
    std::cout << "[";
    int pos = static_cast<int>(barWidth * progress);
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos)        std::cout << "=";
        else if (i == pos)  std::cout << ">";
        else                std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0f) << " %\r";
    std::cout.flush();
}
