#include <ModelLoader.hpp>
#include <tiny_gltf.h>

std::map<std::filesystem::path, std::shared_ptr<tinygltf::Model>> ModelLoader::m_cachedModels{};

Result<ModelLoadResult> ModelLoader::loadModel(const std::filesystem::path& path, bool overrideCache) {
    if (m_cachedModels.count(path)) {
        return Ok(ModelLoadResult{
            .model = m_cachedModels[path],
            .warning = ""
        });
    }

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    bool success = loader.LoadBinaryFromFile(&model, &err, &warn, utils::string::pathToString(path));

    if (!success) {
        return Err(err);
    }

    if (overrideCache || !overrideCache && !m_cachedModels.contains(path)){
        m_cachedModels[path] = std::make_shared<tinygltf::Model>(model);
    }

    return Ok(ModelLoadResult{
        .model = std::make_shared<tinygltf::Model>(model),
        .warning = warn
    });
}

Result<ModelLoadResult> ModelLoader::loadModel(const std::string& resourcePath, bool overrideCache = false){
    
}