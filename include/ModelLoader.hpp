#pragma once

#include <Geode/Geode.hpp>

#include "Types.hpp"

using namespace geode::prelude;

namespace tinygltf { class Model; }

struct RIG_DLL ModelLoadResult {
    std::shared_ptr<tinygltf::Model> model;
    std::string warning;
};

class RIG_DLL ModelLoader {
private:

    static std::map<std::filesystem::path, std::shared_ptr<tinygltf::Model>> m_cachedModels;

    ModelLoader() = default;

public:

    static Result<ModelLoadResult> loadModel(const std::filesystem::path& path, bool overrideCache = false);
    static Result<ModelLoadResult> loadModel(const std::string& resourcePath, bool overrideCache = false);
};