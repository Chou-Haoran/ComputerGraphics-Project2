 #pragma once

#include "BVH.hpp"
#include "Intersection.hpp"
#include "Material.hpp"
#include "OBJ_Loader.hpp"
#include "Object.hpp"
#include "Triangle.hpp"
#include <cassert>
#include <array>
#include <cstring>
#include <stdexcept>

inline Vector3f rotateXYZ(const Vector3f& v, const Vector3f& deg)
{
    Vector3f r = deg * (M_PI / 180.0f);

    float cx = std::cos(r.x), sx = std::sin(r.x);
    float cy = std::cos(r.y), sy = std::sin(r.y);
    float cz = std::cos(r.z), sz = std::sin(r.z);

    Vector3f out = v;
    out = Vector3f(out.x, out.y * cx - out.z * sx, out.y * sx + out.z * cx);
    out = Vector3f(out.x * cy + out.z * sy, out.y, -out.x * sy + out.z * cy);
    out = Vector3f(out.x * cz - out.y * sz, out.x * sz + out.y * cz, out.z);
    return out;
}

inline Vector3f transformPosition(const Vector3f& p,
                                  const Vector3f& scale,
                                  const Vector3f& rotationDeg,
                                  const Vector3f& offset)
{
    return rotateXYZ(Vector3f(p.x * scale.x, p.y * scale.y, p.z * scale.z),
                     rotationDeg) + offset;
}

inline Vector3f transformNormal(const Vector3f& n,
                                const Vector3f& scale,
                                const Vector3f& rotationDeg)
{
    Vector3f invScale(std::fabs(scale.x) > EPSILON ? 1.0f / scale.x : 0.0f,
                      std::fabs(scale.y) > EPSILON ? 1.0f / scale.y : 0.0f,
                      std::fabs(scale.z) > EPSILON ? 1.0f / scale.z : 0.0f);
    return normalize(rotateXYZ(Vector3f(n.x * invScale.x,
                                        n.y * invScale.y,
                                        n.z * invScale.z), rotationDeg));
}


class Triangle : public Object
{
public:
    Vector3f v0, v1, v2; // vertices A, B ,C , counter-clockwise order
    Vector3f e1, e2;     // 2 edges v1-v0, v2-v0;
    Vector2f t0, t1, t2; // texture coords of each vertex
    Vector3f n0, n1, n2; // vertex normals for smooth shading
    Vector3f normal;
    float area;
    Material* m;

    Triangle(Vector3f _v0, Vector3f _v1, Vector3f _v2, Material* _m = nullptr)
        : v0(_v0), v1(_v1), v2(_v2), m(_m)
    {
        e1 = v1 - v0;
        e2 = v2 - v0;
        normal = normalize(crossProduct(e1, e2));
        n0 = normal;
        n1 = normal;
        n2 = normal;
        area = crossProduct(e1, e2).norm()*0.5f;
    }

    void computeTexturePartials(Vector3f& dpdu, Vector3f& dpdv) const
    {
        const float du1 = t1.x - t0.x;
        const float dv1 = t1.y - t0.y;
        const float du2 = t2.x - t0.x;
        const float dv2 = t2.y - t0.y;
        const float det = du1 * dv2 - dv1 * du2;

        if (std::fabs(det) > 1e-8f) {
            dpdu = (e1 * dv2 - e2 * dv1) / det;
            dpdv = (e2 * du1 - e1 * du2) / det;
            return;
        }

        dpdu = e1;
        dpdv = e2;
    }

    void computeTangentFrame(const Vector3f& shadingNormal,
                             Vector3f& tangent,
                             Vector3f& bitangent) const
    {
        computeTexturePartials(tangent, bitangent);
        if (std::fabs(dotProduct(tangent, tangent)) > 1e-8f ||
            std::fabs(dotProduct(bitangent, bitangent)) > 1e-8f) {
            tangent = tangent - shadingNormal * dotProduct(tangent, shadingNormal);
            bitangent = bitangent - shadingNormal * dotProduct(bitangent, shadingNormal);
        } else {
            if (std::fabs(shadingNormal.x) > std::fabs(shadingNormal.z)) {
                tangent = normalize(Vector3f(-shadingNormal.y, shadingNormal.x, 0.0f));
            } else {
                tangent = normalize(Vector3f(0.0f, -shadingNormal.z, shadingNormal.y));
            }
            bitangent = crossProduct(shadingNormal, tangent);
        }

        if (tangent.norm() < EPSILON) {
            tangent = normalize(crossProduct(Vector3f(0.0f, 1.0f, 0.0f), shadingNormal));
            if (tangent.norm() < EPSILON) {
                tangent = normalize(crossProduct(Vector3f(1.0f, 0.0f, 0.0f), shadingNormal));
            }
        } else {
            tangent = normalize(tangent);
        }

        if (bitangent.norm() < EPSILON) {
            bitangent = normalize(crossProduct(shadingNormal, tangent));
        } else {
            bitangent = normalize(bitangent);
        }
    }

    Intersection getIntersection(Ray ray) override;
    void getSurfaceProperties(const Vector3f& P, const Vector3f& I,
                              const uint32_t& index, const Vector2f& uv,
                              Vector3f& N, Vector2f& st) const override
    {
        const float w = 1.0f - uv.x - uv.y;
        N = normalize(n0 * w + n1 * uv.x + n2 * uv.y);
        st = t0 * w + t1 * uv.x + t2 * uv.y;
    }
    Vector3f evalDiffuseColor(const Vector2f&) const override;
    Bounds3 getBounds() override;
    void Sample(Intersection &pos, float &pdf) override {
        float x = std::sqrt(get_random_float()), y = get_random_float();
        const float b0 = 1.0f - x;
        const float b1 = x * (1.0f - y);
        const float b2 = x * y;
        pos.coords = v0 * b0 + v1 * b1 + v2 * b2;
        pos.normal = normalize(n0 * b0 + n1 * b1 + n2 * b2);
        computeTexturePartials(pos.dpdu, pos.dpdv);
        computeTangentFrame(pos.normal, pos.tangent, pos.bitangent);
        pos.tcoords = t0 * b0 + t1 * b1 + t2 * b2;
        pdf = 1.0f / area;
    }
    float getArea() override {
        return area;
    }
    bool hasEmit() override {
        return m->hasEmission();
    }
};

class MeshTriangle : public Object
{
public:
    MeshTriangle(const Vector3f* verts, const uint32_t* vertsIndex, const uint32_t& numTris, const Vector2f* st, Material *mt = nullptr)
    {
        if (!mt) mt = getDefaultMaterial();
        area = 0.0f;
        emissiveArea = 0.0f;
        uint32_t maxIndex = 0;
        for (uint32_t i = 0; i < numTris * 3; ++i)
            if (vertsIndex[i] > maxIndex)
                maxIndex = vertsIndex[i];
        stCoordinates = std::unique_ptr<Vector2f[]>(new Vector2f[maxIndex + 1]);
        memcpy(stCoordinates.get(), st, sizeof(Vector2f) * (maxIndex + 1));
        m=mt;

        Vector3f min_vert = Vector3f{std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity()};
        Vector3f max_vert = Vector3f{-std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity()};
        for (int i = 0; i < numTris; i++) {
            std::array<Vector3f, 3> face_vertices;

            for (int j = 0; j < 3; j++) {
                auto vert = Vector3f(verts[vertsIndex[i*3+j]].x,
                                     verts[vertsIndex[i*3+j]].y,
                                     verts[vertsIndex[i*3+j]].z);
                face_vertices[j] = vert;

                min_vert = Vector3f(std::min(min_vert.x, vert.x),
                                    std::min(min_vert.y, vert.y),
                                    std::min(min_vert.z, vert.z));
                max_vert = Vector3f(std::max(max_vert.x, vert.x),
                                    std::max(max_vert.y, vert.y),
                                    std::max(max_vert.z, vert.z));
            }
            Triangle* tri = new Triangle(face_vertices[0], face_vertices[1],
                                        face_vertices[2], mt);
            tri->t0=st[vertsIndex[i*3]];
            tri->t1=st[vertsIndex[i*3+1]];
            tri->t2=st[vertsIndex[i*3+2]];

            triangles.push_back(*tri);


//            triangles.emplace_back(face_vertices[0], face_vertices[1],
//                                   face_vertices[2], mt);
        }

        bounding_box = Bounds3(min_vert, max_vert);

        std::vector<Object*> ptrs;
        for (auto& tri : triangles){
            ptrs.push_back(&tri);
            area += tri.area;
            if (tri.m && tri.m->hasEmission()) emissiveArea += tri.area;
        }
        if (ENABLE_BVH) {
            bvh = new BVHAccel(ptrs);
        } else {
            bvh = nullptr;
        }
    }

    MeshTriangle(const std::string& filename, Vector3f offset, Material *mt = nullptr)
    {
        if (!mt) mt = getDefaultMaterial();
        objl::Loader loader;
        if (!loader.LoadFile(filename)) {
            throw std::runtime_error("MeshTriangle: failed to load OBJ: " + filename);
        }
        std::vector<Material*> meshMaterials(loader.LoadedMeshes.size(), mt);
        buildFromLoader(loader, offset, Vector3f(0.0f), Vector3f(1.0f), meshMaterials);
    }

    MeshTriangle(const objl::Loader& loader,
                 Vector3f offset,
                 Vector3f rotation,
                 Vector3f scale,
                 const std::vector<Material*>& meshMaterials)
    {
        buildFromLoader(loader, offset, rotation, scale, meshMaterials);
    }


    Bounds3 getBounds() { return bounding_box; }

    void getSurfaceProperties(const Vector3f& P, const Vector3f& I,
                              const uint32_t& index, const Vector2f& uv,
                              Vector3f& N, Vector2f& st) const
    {
        const Vector3f& v0 = vertices[vertexIndex[index * 3]];
        const Vector3f& v1 = vertices[vertexIndex[index * 3 + 1]];
        const Vector3f& v2 = vertices[vertexIndex[index * 3 + 2]];
        Vector3f e0 = normalize(v1 - v0);
        Vector3f e1 = normalize(v2 - v1);
        N = normalize(crossProduct(e0, e1));
        const Vector2f& st0 = stCoordinates[vertexIndex[index * 3]];
        const Vector2f& st1 = stCoordinates[vertexIndex[index * 3 + 1]];
        const Vector2f& st2 = stCoordinates[vertexIndex[index * 3 + 2]];
        st = st0 * (1 - uv.x - uv.y) + st1 * uv.x + st2 * uv.y;
    }

    Vector3f evalDiffuseColor(const Vector2f& st) const
    {
        return m->getColorAt(st.x, st.y);
    }

    Intersection getIntersection(Ray ray)
    {
        Intersection intersec;

        if (ENABLE_BVH && bvh) {
            intersec = bvh->Intersect(ray);
        } else {
            for (auto& tri : triangles) {
                Intersection hit = tri.getIntersection(ray);
                if (hit.happened && hit.tnear < intersec.tnear) {
                    intersec = hit;
                }
            }
        }

        return intersec;
    }
    
    void Sample(Intersection &pos, float &pdf){
        if (emissiveArea > 0.0f) {
            float p = get_random_float() * emissiveArea;
            float areaSum = 0.0f;
            for (auto& tri : triangles) {
                if (!tri.m || !tri.m->hasEmission()) continue;
                areaSum += tri.area;
                if (p <= areaSum) {
                    tri.Sample(pos, pdf);
                    pos.obj = &tri;
                    pos.material = tri.m;
                    pdf *= tri.area;
                    pdf /= emissiveArea;
                    return;
                }
            }
        }

        if (ENABLE_BVH && bvh) {
            bvh->Sample(pos, pdf);
        } else {
            float p = get_random_float() * area;
            float areaSum = 0.0f;
            for (auto& tri : triangles) {
                areaSum += tri.area;
                if (p <= areaSum) {
                    tri.Sample(pos, pdf);
                    pdf *= tri.area;
                    break;
                }
            }
            pdf /= area;
        }
        pos.obj=this;
        pos.material=this->m;
    }
    float getArea(){
        return area;
    }
    bool hasEmit(){
        return emissiveArea > 0.0f;
    }

    Bounds3 bounding_box;
    std::unique_ptr<Vector3f[]> vertices;
    uint32_t numTriangles;
    std::unique_ptr<uint32_t[]> vertexIndex;
    std::unique_ptr<Vector2f[]> stCoordinates;

    std::vector<Triangle> triangles;

    BVHAccel* bvh;
    float area;
    float emissiveArea = 0.0f;

    Material* m;

private:
    void buildFromLoader(const objl::Loader& loader,
                         Vector3f offset,
                         Vector3f rotation,
                         Vector3f scale,
                         const std::vector<Material*>& meshMaterials)
    {
        area = 0.0f;
        emissiveArea = 0.0f;
        triangles.clear();
        m = meshMaterials.empty() ? nullptr : meshMaterials.front();

        Vector3f min_vert = Vector3f{std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::infinity()};
        Vector3f max_vert = Vector3f{-std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity(),
                                     -std::numeric_limits<float>::infinity()};
        for (size_t meshIdx = 0; meshIdx < loader.LoadedMeshes.size(); ++meshIdx) {
            const auto& mesh = loader.LoadedMeshes[meshIdx];
            Material* meshMaterial =
                meshIdx < meshMaterials.size() && meshMaterials[meshIdx] != nullptr
                    ? meshMaterials[meshIdx]
                    : m;
            for (int i = 0; i + 2 < static_cast<int>(mesh.Indices.size()); i += 3) {
                std::array<Vector3f, 3> face_vertices;
                std::array<unsigned int, 3> face_indices = {
                    mesh.Indices[i + 0],
                    mesh.Indices[i + 1],
                    mesh.Indices[i + 2]
                };

                for (int j = 0; j < 3; j++) {
                    const auto& src = mesh.Vertices[face_indices[j]];
                    auto vert = transformPosition(Vector3f(src.Position.X,
                                                           src.Position.Y,
                                                           src.Position.Z),
                                                  scale, rotation, offset);
                    face_vertices[j] = vert;

                    min_vert = Vector3f(std::min(min_vert.x, vert.x),
                                        std::min(min_vert.y, vert.y),
                                        std::min(min_vert.z, vert.z));
                    max_vert = Vector3f(std::max(max_vert.x, vert.x),
                                        std::max(max_vert.y, vert.y),
                                        std::max(max_vert.z, vert.z));
                }

                triangles.emplace_back(face_vertices[0], face_vertices[1],
                                       face_vertices[2], meshMaterial);
                triangles.back().t0 = Vector2f(mesh.Vertices[face_indices[0]].TextureCoordinate.X,
                                               mesh.Vertices[face_indices[0]].TextureCoordinate.Y);
                triangles.back().t1 = Vector2f(mesh.Vertices[face_indices[1]].TextureCoordinate.X,
                                               mesh.Vertices[face_indices[1]].TextureCoordinate.Y);
                triangles.back().t2 = Vector2f(mesh.Vertices[face_indices[2]].TextureCoordinate.X,
                                               mesh.Vertices[face_indices[2]].TextureCoordinate.Y);
                triangles.back().n0 = transformNormal(Vector3f(mesh.Vertices[face_indices[0]].Normal.X,
                                                               mesh.Vertices[face_indices[0]].Normal.Y,
                                                               mesh.Vertices[face_indices[0]].Normal.Z),
                                                      scale, rotation);
                triangles.back().n1 = transformNormal(Vector3f(mesh.Vertices[face_indices[1]].Normal.X,
                                                               mesh.Vertices[face_indices[1]].Normal.Y,
                                                               mesh.Vertices[face_indices[1]].Normal.Z),
                                                      scale, rotation);
                triangles.back().n2 = transformNormal(Vector3f(mesh.Vertices[face_indices[2]].Normal.X,
                                                               mesh.Vertices[face_indices[2]].Normal.Y,
                                                               mesh.Vertices[face_indices[2]].Normal.Z),
                                                      scale, rotation);
            }
        }

        if (triangles.empty()) {
            throw std::runtime_error("MeshTriangle: no triangles found in OBJ loader");
        }

        bounding_box = Bounds3(min_vert, max_vert);

        std::vector<Object*> ptrs;
        for (auto& tri : triangles){
            ptrs.push_back(&tri);
            area += tri.area;
            if (tri.m && tri.m->hasEmission()) emissiveArea += tri.area;
        }
        if (ENABLE_BVH) {
            bvh = new BVHAccel(ptrs);
        } else {
            bvh = nullptr;
        }
    }
};


inline Bounds3 Triangle::getBounds() { return Union(Bounds3(v0, v1), v2); }

inline Intersection Triangle::getIntersection(Ray ray)
{
    // TODO: task 1.1 Ray-Triangle Intersection (Möller–Trumbore intersection algorithm)
    Intersection inter;
    constexpr float kRayTriangleEpsilon = 1e-8f;

    Vector3f pvec = crossProduct(ray.direction, e2);
    float det = dotProduct(e1, pvec);
    if (std::fabs(det) < kRayTriangleEpsilon) {
        return inter;
    }

    float invDet = 1.0f / det;
    Vector3f tvec = ray.origin - v0;
    float u = dotProduct(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return inter;
    }

    Vector3f qvec = crossProduct(tvec, e1);
    float v = dotProduct(ray.direction, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return inter;
    }

    float t = dotProduct(e2, qvec) * invDet;
    if (t < kRayTriangleEpsilon) {
        return inter;
    }

    inter.happened = true;
    const float w = 1.0f - u - v;
    inter.coords = ray.origin + ray.direction * t;
    inter.normal = normalize(n0 * w + n1 * u + n2 * v);
    computeTexturePartials(inter.dpdu, inter.dpdv);
    computeTangentFrame(inter.normal, inter.tangent, inter.bitangent);
    inter.tcoords = t0 * w + t1 * u + t2 * v;
    inter.tnear = t;
    inter.obj = this;
    inter.material = m;
    return inter;
}

inline Vector3f Triangle::evalDiffuseColor(const Vector2f& st) const
{
    return m->getColorAt(st.x, st.y);
}
