#include "MaterialFactory.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

MaterialFactory& MaterialFactory::instance()
{
    static MaterialFactory factory;
    return factory;
}

bool MaterialFactory::registerType(const std::string& typeName, Creator creator)
{
    return creators_.emplace(normalizeTypeName(typeName), std::move(creator)).second;
}

bool MaterialFactory::isRegistered(const std::string& typeName) const
{
    return creators_.find(normalizeTypeName(typeName)) != creators_.end();
}

std::unique_ptr<Material> MaterialFactory::create(const std::string& typeName,
                                                  const MaterialConfig& config) const
{
    auto it = creators_.find(normalizeTypeName(typeName));
    if (it == creators_.end()) {
        std::ostringstream oss;
        oss << "unknown material type \"" << typeName << "\"";
        throw std::runtime_error(oss.str());
    }
    return it->second(config);
}

std::vector<std::string> MaterialFactory::registeredTypes() const
{
    std::vector<std::string> names;
    names.reserve(creators_.size());
    for (const auto& [name, unused] : creators_) {
        (void)unused;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string MaterialFactory::normalizeTypeName(std::string typeName)
{
    std::transform(typeName.begin(), typeName.end(), typeName.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return typeName;
}

Material* getDefaultMaterial()
{
    static std::unique_ptr<Material> material =
        MaterialFactory::instance().create("DIFFUSE", MaterialConfig{});
    return material.get();
}
