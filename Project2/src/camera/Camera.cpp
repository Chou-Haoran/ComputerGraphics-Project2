#include "Camera.hpp"
#include "global.hpp"
#include <cmath>

Camera::Camera(Vector3f pos,
               Vector3f lookAt,
               Vector3f up,
               float    fovDeg,
               float    aspect,
               float    apertureIn,
               float    focusDistIn)
    : origin(pos),
      aperture(apertureIn),
      focusDist(focusDistIn),
      fovDegrees(fovDeg),
      aspectRatio(aspect)
{
    // Build a right-handed orthonormal basis so the camera looks at `lookAt`.
    forward = normalize(lookAt - pos);
    right   = normalize(crossProduct(forward, up));
    upDir   = crossProduct(right, forward);

    float theta = deg2rad(fovDeg);
    halfHeight  = std::tan(theta * 0.5f);
    halfWidth   = aspectRatio * halfHeight;
}

void Camera::setAspect(float aspect)
{
    aspectRatio = aspect;
    halfWidth   = aspectRatio * halfHeight;
}

Ray Camera::generateRay(float u, float v, float r1, float r2) const
{
    // Pinhole path (aperture == 0): direct ray from `origin` through the
    // pixel sample on the image plane (placed at z = 1 in camera space).
    float ndcX = (2.0f * u - 1.0f) * halfWidth;
    float ndcY = (1.0f - 2.0f * v) * halfHeight;

    Vector3f dir = normalize(forward + right * ndcX + upDir * ndcY);

    if (aperture <= 0.0f) {
        return Ray(origin, dir);
    }

    // TODO: thin-lens depth-of-field sampling.
    // Steps:
    //   1) sample a 2D point on a disk of radius (aperture/2) using r1, r2
    //      (concentric or rejection sampling).
    //   2) offset the ray origin by (offsetX * right + offsetY * upDir).
    //   3) the focal point lies along `dir` at distance focusDist from origin;
    //      new direction = normalize(focalPoint - newOrigin).
    // For now we ignore r1/r2 and behave like a pinhole so the build stays
    // valid.
    (void)r1; (void)r2;
    return Ray(origin, dir);
}
