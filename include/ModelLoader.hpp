#pragma once

#include <Geode/Geode.hpp>
#include "tiny_gltf_v3.h"

#include "Types.hpp"

using namespace geode::prelude;

struct RIG_DLL RigGLTFModel : tg3_model {
    std::vector<std::vector<std::uint8_t>> imageData;

    RigGLTFModel();
    ~RigGLTFModel();

    RigGLTFModel(const RigGLTFModel&) = delete;
    RigGLTFModel& operator=(const RigGLTFModel&) = delete;
};

struct ModelLoadResult {
    std::shared_ptr<RigGLTFModel> model;
    std::string warning;
};

class RIG_DLL ModelLoader {
private:

    static std::map<asp::fs::Path, std::shared_ptr<RigGLTFModel>> m_cachedModels;

    ModelLoader() = default;

    static void loadImageData(RigGLTFModel& model, const asp::fs::Path& modelPath, std::string& warnings);

    static int hexValue(char c);

    static bool decodePercentEscapes(std::string_view input, std::vector<std::uint8_t>& output);

    static bool decodeDataUri(std::string_view uri, std::vector<std::uint8_t>& output);

public:

    static Result<ModelLoadResult> loadModel(const asp::fs::Path& path, bool overrideCache = false);
    static Result<ModelLoadResult> loadModel(const std::string& resourcePath, bool overrideCache = false);
};