#ifndef PROJECT2_CAMERA_H
#define PROJECT2_CAMERA_H

#include "Vector.hpp"
#include "Ray.hpp"

// Parameterized camera satisfying PDF user-control requirement (i):
// camera position and orientation. Supports thin-lens depth-of-field
// (TODO) to deliver the motivational image's macro-photography look.
class Camera {
public:
    Camera(Vector3f pos,
           Vector3f lookAt,
           Vector3f up,
           float    fovDeg,
           float    aspect,
           float    aperture  = 0.0f,
           float    focusDist = 1.0f);

    // u, v in [0, 1] (image-space sample); r1, r2 in [0, 1] (lens sample).
    // Returns a primary ray in world space.
    Ray generateRay(float u, float v, float r1, float r2) const;

    // Recomputes the image-plane half-extents after a resolution change.
    void setAspect(float aspect);

    Vector3f position() const { return origin; }
    float    fov()      const { return fovDegrees; }
    float    aspect()   const { return aspectRatio; }
    Vector3f forwardDir() const { return forward; }
    Vector3f rightDir()   const { return right; }
    Vector3f upVector()   const { return upDir; }

private:
    Vector3f origin;
    Vector3f forward, right, upDir;
    float    halfWidth, halfHeight;
    float    aperture;
    float    focusDist;
    float    fovDegrees;
    float    aspectRatio;
};

#endif // PROJECT2_CAMERA_H
