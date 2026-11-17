#include <ModelLoader.hpp>

#include "ModelUtils.hpp"

#include <asp/fs.hpp>

std::map<asp::fs::Path, std::shared_ptr<RigGLTFModel>> ModelLoader::m_cachedModels{};

RigGLTFModel::RigGLTFModel() : tg3_model{} {
    default_scene = -1;
}

RigGLTFModel::~RigGLTFModel() {
    tg3_model_free(this);
}

Result<ModelLoadResult> ModelLoader::loadModel(const asp::fs::Path& path, bool overrideCache) {
    if (m_cachedModels.count(path)) {
        return Ok(ModelLoadResult{
            .model = m_cachedModels[path],
            .warning = ""
        });
    }

    auto model = std::make_shared<RigGLTFModel>();
    tinygltf3::ErrorStack errors;
    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.images_as_is = 1;

    auto filename = path.string();
    tg3_error_code result = tg3_parse_file(
        model.get(),
        errors.get(),
        filename.c_str(),
        static_cast<uint32_t>(filename.size()),
        &options
    );

    std::string err;
    std::string warn;
    for (auto i = 0; i < errors.count(); i++) {
        auto entry = errors.entry(i);
        if (!entry || !entry->message) continue;

        if (entry->severity != TG3_SEVERITY_ERROR && entry->severity != TG3_SEVERITY_WARNING) continue;

        std::string& message = entry->severity == TG3_SEVERITY_ERROR ? err : warn;
        
        if (!message.empty()) message += '\n';
        message += entry->message;
    }

    if (result != TG3_OK || errors.has_error()) {
        if (err.empty()) err = "TinyGLTF v3 failed to parse the model.";
        return Err(err);
    }

    loadImageData(*model, path, warn);

    if (overrideCache || !m_cachedModels.contains(path)) m_cachedModels[path] = model;

    return Ok(ModelLoadResult{
        .model = std::move(model),
        .warning = warn
    });
}

Result<ModelLoadResult> ModelLoader::loadModel(const std::string& resourcePath, bool overrideCache) {
    asp::fs::Path path = resourcePath;
    auto resourcesDir = dirs::getResourcesDir();

    auto fullPath = resourcesDir / path;
    if (fullPath.exists()) {
        return loadModel(fullPath, overrideCache);
    }

    auto components = path.components();
    const auto firstDir = components.next();
    if (firstDir) {
        path = asp::fs::Path(dirs::getModRuntimeDir().string()) / *firstDir / "resources" / path;

        if (path.exists()) {
            return loadModel(path, overrideCache);
        }
    }

    return Err("Resource path invalid!");
}

void ModelLoader::loadImageData(RigGLTFModel& model, const asp::fs::Path& modelPath, std::string& warnings) {
    model.imageData.resize(model.images_count);

    auto appendWarning = [&](const std::string& message) {
        if (!warnings.empty()) warnings += '\n';
        warnings += message;
    };

    for (uint32_t i = 0; i < model.images_count; ++i) {
        const auto& image = model.images[i];
        auto& data = model.imageData[i];

        if (image.buffer_view >= 0) {
            if (static_cast<uint32_t>(image.buffer_view) >= model.buffer_views_count) {
                appendWarning(fmt::format("Image {} has an invalid bufferView.", i));
                continue;
            }

            const auto& view = model.buffer_views[image.buffer_view];
            if (view.buffer < 0 || static_cast<uint32_t>(view.buffer) >= model.buffers_count) {
                appendWarning(fmt::format("Image {} references an invalid buffer.", i));
                continue;
            }

            const auto& buffer = model.buffers[view.buffer].data;
            if (view.byte_offset > buffer.count || view.byte_length > buffer.count - view.byte_offset) {
                appendWarning(fmt::format("Image {} exceeds its buffer bounds.", i));
                continue;
            }

            data.assign(buffer.data + view.byte_offset, buffer.data + view.byte_offset + view.byte_length);
            continue;
        }

        auto uri = ModelUtils::toString(image.uri);
        if (uri.empty()) {
            appendWarning(fmt::format("Image {} has no URI or bufferView.", i));
            continue;
        }

        if (uri.compare(0, 5, "data:") == 0) {
            if (!decodeDataUri(uri, data)) {
                appendWarning(fmt::format("Image {} has an invalid data URI.", i));
            }
            continue;
        }

        std::vector<std::uint8_t> decodedUri;
        if (!decodePercentEscapes(uri, decodedUri)) {
            appendWarning(fmt::format("Image {} has an invalid URI escape.", i));
            continue;
        }
        
        std::string imagePathString(decodedUri.begin(), decodedUri.end());
        asp::fs::Path imagePath(imagePathString);

        auto resolvedPath = imagePath;

        if (!imagePath.isAbsolute()) {
            if (const auto modelParent = modelPath.parent()) {
                resolvedPath = asp::fs::Path(modelParent.value()) / imagePath;
            }
        }

        auto res = asp::fs::read(resolvedPath);
        if (res.isErr()) {
            appendWarning(fmt::format("Unable to read image {} from '{}'.", i, resolvedPath));
            continue;
        }

        data = std::move(res).unwrap();
        if (data.empty()) {
            appendWarning(fmt::format("Image {} is empty.", i));
        }
    }
}

int ModelLoader::hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;

    return -1;
}

bool ModelLoader::decodePercentEscapes(std::string_view input, std::vector<std::uint8_t>& output) {
    output.clear();

    for (auto i = 0; i < input.size(); i++) {
        if (input[i] != '%') {
            output.push_back(static_cast<std::uint8_t>(input[i]));
            continue;
        }

        if (i + 2 >= input.size()) return false;

        auto high = hexValue(input[i + 1]);
        auto low = hexValue(input[i + 2]);

        if (high < 0 || low < 0) return false;

        output.push_back(static_cast<std::uint8_t>((high << 4) | low));

        i += 2;
    }

    return true;
}

bool ModelLoader::decodeDataUri(std::string_view uri, std::vector<std::uint8_t>& output) {
    auto comma = uri.find(',');

    if (comma == std::string_view::npos || uri.substr(0, 5) != "data:") return false;

    std::string metadata(uri.substr(5, comma - 5));

    std::transform(metadata.begin(), metadata.end(), metadata.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    auto payload = uri.substr(comma + 1);

    if (metadata.size() < 7 || metadata.compare(metadata.size() - 7, 7, ";base64") != 0) {
        return decodePercentEscapes(payload, output);
    }

    output.clear();

    std::uint32_t accumulator = 0;
    int bits = 0;
    bool padding = false;
    constexpr std::string_view base64Alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (unsigned char c : payload) {
        if (std::isspace(c)) continue;

        if (c == '=') {
            padding = true;
            continue;
        }
        if (padding) return false;

        const auto value = base64Alphabet.find(static_cast<char>(c));
        if (value == std::string_view::npos) return false;

        accumulator = (accumulator << 6) | static_cast<std::uint32_t>(value);
        bits += 6;

        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 255));
        }
    }
    
    return bits < 6;
}
