#pragma once

#ifndef PROJECT2_LIGHT_TYPE_H
#define PROJECT2_LIGHT_TYPE_H

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

enum class LightType {
    AreaRect,
    AreaDisk,
    AreaSphere,
    Point,
    Spot,
    Directional,
    MeshEmit,
};

inline std::string normalizeLightTypeName(std::string typeName)
{
    std::transform(typeName.begin(), typeName.end(), typeName.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });
    return typeName;
}

inline LightType parseLightTypeName(const std::string& typeName)
{
    const std::string normalized = normalizeLightTypeName(typeName);
    if (normalized == "AREA_RECT" || normalized == "AREA") return LightType::AreaRect;
    if (normalized == "AREA_DISK") return LightType::AreaDisk;
    if (normalized == "AREA_SPHERE") return LightType::AreaSphere;
    if (normalized == "POINT") return LightType::Point;
    if (normalized == "SPOT") return LightType::Spot;
    if (normalized == "DIRECTIONAL") return LightType::Directional;
    if (normalized == "MESH_EMIT") return LightType::MeshEmit;
    throw std::invalid_argument("unknown light type \"" + typeName + "\"");
}

inline const char* lightTypeName(LightType type)
{
    switch (type) {
    case LightType::AreaRect:    return "AREA_RECT";
    case LightType::AreaDisk:    return "AREA_DISK";
    case LightType::AreaSphere:  return "AREA_SPHERE";
    case LightType::Point:       return "POINT";
    case LightType::Spot:        return "SPOT";
    case LightType::Directional: return "DIRECTIONAL";
    case LightType::MeshEmit:    return "MESH_EMIT";
    }
    return "AREA_RECT";
}

#endif // PROJECT2_LIGHT_TYPE_H
