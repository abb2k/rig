#pragma once

#include <Geode/Geode.hpp>

#include <tiny_gltf.h>
#include <Types.hpp>

using namespace geode::prelude;

struct ModelLoadResult {
    std::shared_ptr<tinygltf::Model> model;
    std::string warning;
};

class ModelLoader {
private:

    static std::map<std::filesystem::path, std::shared_ptr<tinygltf::Model>> m_cachedModels;

    ModelLoader() = default;

public:

    static Result<ModelLoadResult> loadModel(const std::filesystem::path& path, bool overrideCache = false);
};