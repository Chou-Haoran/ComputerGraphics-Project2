#ifndef PROJECT2_MATERIAL_FACTORY_H
#define PROJECT2_MATERIAL_FACTORY_H

#include "Material.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MaterialFactory {
public:
    using Creator = std::function<std::unique_ptr<Material>(const MaterialConfig&)>;

    static MaterialFactory& instance();

    bool registerType(const std::string& typeName, Creator creator);
    bool isRegistered(const std::string& typeName) const;
    std::unique_ptr<Material> create(const std::string& typeName,
                                     const MaterialConfig& config = {}) const;
    std::vector<std::string> registeredTypes() const;

    static std::string normalizeTypeName(std::string typeName);

private:
    std::unordered_map<std::string, Creator> creators_;
};

template <typename T>
class MaterialRegistrar {
public:
    explicit MaterialRegistrar(const char* typeName)
    {
        MaterialFactory::instance().registerType(
            typeName,
            [](const MaterialConfig& config) {
                return std::make_unique<T>(config);
            });
    }
};

#endif // PROJECT2_MATERIAL_FACTORY_H
