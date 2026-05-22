//
// Created by LEI XU on 5/16/19.
//

#ifndef RAYTRACING_INTERSECTION_H
#define RAYTRACING_INTERSECTION_H
#include "Vector.hpp"
#include "Material.hpp"
class Object;
class Sphere;

struct Intersection
{
    Intersection(){
        happened=false;
        coords=Vector3f();
        tcoords=Vector2f();
        normal=Vector3f();
        tangent=Vector3f();
        bitangent=Vector3f();
        dpdu=Vector3f();
        dpdv=Vector3f();
        tnear= std::numeric_limits<double>::max();
        obj =nullptr;
        material=nullptr;
    }
    bool happened;
    Vector3f coords;
    Vector2f tcoords;
    Vector3f normal;
    Vector3f tangent;
    Vector3f bitangent;
    Vector3f dpdu;
    Vector3f dpdv;
    double tnear;  // the distance between the intersection point and the ray origin
    Object* obj;
    Material* material;
};
#endif //RAYTRACING_INTERSECTION_H
