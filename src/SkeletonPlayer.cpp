#include <ModelLoader.hpp>
#include <SkeletonPlayer.hpp>

#include <ModelUtils.hpp>
#include <bit>

SkeletonPlayer* SkeletonPlayer::create() {
    auto ret = new SkeletonPlayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool SkeletonPlayer::init() {
    if (!CCNode::init()) return false;
    this->setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor));
    return true;
}

void SkeletonPlayer::onEnter() {
    CCNode::onEnter();
    this->scheduleUpdate();
}

void SkeletonPlayer::onExit() {
    CCNode::onExit();
    this->unscheduleUpdate();
}

SkeletonPlayer::~SkeletonPlayer() {
    for (auto& [id, bone] : m_embeddedTextures) CC_SAFE_RELEASE(bone);
    for (auto& [id, bone] : m_customTextures) CC_SAFE_RELEASE(bone);
    for (auto& [id, frame] : m_customFrames) CC_SAFE_RELEASE(frame);
}

void SkeletonPlayer::setColor(const ccColor3B& color) {
    m_color = color;
}

const ccColor3B& SkeletonPlayer::getColor() {
    return m_color;
}

const ccColor3B& SkeletonPlayer::getDisplayedColor() {
    return m_color;
}

GLubyte SkeletonPlayer::getDisplayedOpacity() {
    return m_opacity;
}

GLubyte SkeletonPlayer::getOpacity(void) {
    return m_opacity;
}

void SkeletonPlayer::setOpacity(GLubyte opacity) {
    m_opacity = opacity;
}

void SkeletonPlayer::setOpacityModifyRGB(bool bValue) {
    m_opacityModifyRGB = bValue;
}

bool SkeletonPlayer::isOpacityModifyRGB() {
    return m_opacityModifyRGB;
}

bool SkeletonPlayer::isCascadeColorEnabled() {
    return m_cascadeColorEnabled;
}
void SkeletonPlayer::setCascadeColorEnabled(bool cascadeColorEnabled) {
    m_cascadeColorEnabled = cascadeColorEnabled;
}

void SkeletonPlayer::updateDisplayedColor(const ccColor3B& color) {
    m_color = color;
}

bool SkeletonPlayer::isCascadeOpacityEnabled() {
    return m_cascadeOpacityEnabled;
}
void SkeletonPlayer::setCascadeOpacityEnabled(bool cascadeOpacityEnabled) {
    m_cascadeOpacityEnabled = cascadeOpacityEnabled;
}

void SkeletonPlayer::updateDisplayedOpacity(GLubyte opacity) {
    m_opacity = opacity;
}

void SkeletonPlayer::togglePause() {
    m_paused = !m_paused;
}

void SkeletonPlayer::setTextureForMesh(const std::string& meshName, CCTexture2D* tex) {
    if (m_customTextures.count(meshName)) {
        CC_SAFE_RELEASE(m_customTextures[meshName]);
    }

    if (m_customFrames.count(meshName)) {
        CC_SAFE_RELEASE(m_customFrames[meshName]);
        m_customFrames.erase(meshName);
    }

    if (tex) {
        tex->retain();
    }

    m_customTextures[meshName] = tex;
}

void SkeletonPlayer::setSpriteForMesh(const std::string& meshName, const std::string& spriteName) {
    auto tex = CCTextureCache::sharedTextureCache()->addImage(spriteName.c_str(), false);

    setTextureForMesh(meshName, tex);
}

void SkeletonPlayer::setSpriteForMeshWithFrameName(const std::string& meshName, const std::string& frameName) {
    auto frame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frameName.c_str());
    if (!frame) return;

    setTextureForMesh(meshName, frame->getTexture());
    
    frame->retain();
    m_customFrames[meshName] = frame;
}

void SkeletonPlayer::loadFromGLTF(const std::shared_ptr<RigGLTFModel>& model) {
    m_model = model;
    m_loaded = false;
    
    m_time = 0.f;
    m_maxAnimTime = 0.f;
    m_tracks.clear(); 

    loadEmbeddedTextures();
    buildBoneNodes();
    extractMeshes();
    extractAnimations();

    m_globalJointMatrices.resize(m_inverseBindMatrices.size());
    m_loaded = true;
}

void SkeletonPlayer::update(float dt) {
    if (!m_loaded || m_paused) return;

    float scaledDT = dt * m_animSpeedModifier;

    if (m_blendingWeight < 1.0f && m_blendingDuration > 0.0f) {
        m_blendingTimer += scaledDT;
        m_blendingWeight = std::min(1.0f, m_blendingTimer / m_blendingDuration);
        
        if (m_blendingWeight >= 1.0f) {
            m_prevAnimName = "";
        }
    }

    if (m_animations.count(m_currentAnimName)) {
        float duration = m_animations[m_currentAnimName].duration;
        m_time += scaledDT;
        if (duration > 0) m_time = fmod(m_time, duration);
    }

    if (m_blendingWeight < 1.0f && m_animations.count(m_prevAnimName)) {
        float prevDuration = m_animations[m_prevAnimName].duration;
        m_prevAnimTime += scaledDT;
        if (prevDuration > 0) m_prevAnimTime = fmod(m_prevAnimTime, prevDuration);
    }

    if (m_animations.count(m_currentAnimName)) {
        auto& currentAnim = m_animations[m_currentAnimName];
        
        bool isBlending = (m_blendingWeight < 1.0f && m_animations.count(m_prevAnimName));
        
        for (auto& [nodeIdx, bone] : m_boneNodes) {
            auto finalPos = bone->getLocalTranslation();
            auto finalRotation = bone->getLocalRotation();
            auto finalScale = bone->getLocalScale();

            auto getAnimTRS = [&](const Animation& anim, float animTime, Vec3& outT, Quat& outR, Vec3& outS) {
                for (auto& track : anim.tracks) {
                    if (track.targetNode != nodeIdx) continue;
                    
                    auto val = sampleTrack(track, animTime); 
                    if (val.empty()) continue;

                    if (track.path == GLB_ANIM_PATH_TRANSLATION) outT = {val[0], val[1], val[2]};
                    else if (track.path == GLB_ANIM_PATH_ROTATION) outR = {val[0], val[1], val[2], val[3]};
                    else if (track.path == GLB_ANIM_PATH_SCALE) outS = {val[0], val[1], val[2]};
                }
            };

            if (isBlending) {
                Vec3 transform1 = finalPos;
                Vec3 transform2 = finalPos;
                Quat rotation1 = finalRotation;
                Quat rotation2 = finalRotation;
                Vec3 scale1 = finalScale;
                Vec3 scale2 = finalScale;

                getAnimTRS(m_animations[m_prevAnimName], m_prevAnimTime, transform1, rotation1, scale1);
                getAnimTRS(currentAnim, m_time, transform2, rotation2, scale2);

                finalPos = Vec3::lerp(transform1, transform2, m_blendingWeight);
                finalRotation = Quat::nlerp(rotation1, rotation2, m_blendingWeight);
                finalScale = Vec3::lerp(scale1, scale2, m_blendingWeight);
            } else {
                getAnimTRS(currentAnim, m_time, finalPos, finalRotation, finalScale);
            }

            bone->updateFromGLTF(finalPos, finalRotation, finalScale);
        }
    }

    for (auto& [id, bone] : m_boneNodes) {
        bone->syncFromCCNode();
    }

    int sceneIndex = m_model->default_scene > -1 ? m_model->default_scene : 0;
    
    if (sceneIndex >= 0 && static_cast<uint32_t>(sceneIndex) < m_model->scenes_count) {
        const auto& scene = m_model->scenes[sceneIndex];
        for (uint32_t i = 0; i < scene.nodes_count; ++i) {
            recalcGlobalMatrices(scene.nodes[i], Mat4());
        }
    }

    applySkinning();
}

void SkeletonPlayer::draw() {
    if (!m_loaded) return;
    auto shader = this->getShaderProgram();
    if (!shader) return;

    CC_NODE_DRAW_SETUP();
    ccGLEnableVertexAttribs(kCCVertexAttribFlag_PosColorTex);
    shader->use();
    shader->setUniformsForBuiltins();

    ccGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GLboolean wasCullEnabled;
    glGetBooleanv(GL_CULL_FACE, &wasCullEnabled);
    glDisable(GL_CULL_FACE);

    std::vector<std::pair<int, float>> depthSortedMeshes{};
    for (size_t i = 0; i < m_subMeshes.size(); i++) {
        float avgZ = 0.f;

        if (!m_subMeshes[i].renderVertices.empty()) {
            for (auto& vert : m_subMeshes[i].renderVertices) {
                avgZ += vert.pos.z; 
            }

            avgZ /= m_subMeshes[i].renderVertices.size();
        }

        depthSortedMeshes.push_back({(int)i, avgZ});
    }

    std::sort(depthSortedMeshes.begin(), depthSortedMeshes.end(), [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
        return a.second < b.second;
    });

    for (auto& [id, _] : depthSortedMeshes) {
        auto& sub = m_subMeshes[id];
        if (sub.indices.empty() || sub.renderVertices.empty()) continue;

        glVertexAttribPointer(kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), &sub.renderVertices[0].pos);
        glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), &sub.renderVertices[0].uv);
        glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), &sub.renderVertices[0].color);

        CCTexture2D* activeTex = nullptr;

        if (m_customTextures.count(sub.nodeName)) {
            activeTex = m_customTextures[sub.nodeName];
        } else if (sub.embeddedImageIndex >= 0 && m_embeddedTextures.count(sub.embeddedImageIndex)) {
            activeTex = m_embeddedTextures[sub.embeddedImageIndex];
        }

        if (activeTex) {
            ccGLBindTexture2D(activeTex->getName());

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } else {
            ccGLBindTexture2D(CCTextureCache::sharedTextureCache()->addImage("fallback.png"_spr, false)->getName());
        }

        glDrawElements(GL_TRIANGLES, sub.indices.size(), GL_UNSIGNED_SHORT, sub.indices.data());
    }

    if (wasCullEnabled) glEnable(GL_CULL_FACE);
}

void SkeletonPlayer::loadEmbeddedTextures() {
    for (auto i = 0; i < m_model->images_count; i++) {
        auto& data = m_model->imageData[i];
        if (data.empty() || data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) continue;

        auto image = new CCImage();
        if (!image->initWithImageData(data.data(), static_cast<int>(data.size()))) {
            image->release();
            continue;
        }

        auto texture = new CCTexture2D();
        auto pixelFormat = image->hasAlpha() ? kCCTexture2DPixelFormat_RGBA8888 : kCCTexture2DPixelFormat_RGB888;

        if (!texture->initWithData(
                image->getData(),
                pixelFormat,
                image->getWidth(),
                image->getHeight(),
                CCSize(image->getWidth(), image->getHeight()))) {

            texture->release();
            image->release();
            continue;
        }
        image->release();
        m_embeddedTextures[i] = texture;
    }
}

void SkeletonPlayer::extractMeshes() {
    m_subMeshes.clear();
    
    for (auto nIdx = 0; nIdx < m_model->nodes_count; nIdx++) {
        auto& gltfNode = m_model->nodes[nIdx];
        
        if (gltfNode.mesh < 0 || static_cast<uint32_t>(gltfNode.mesh) >= m_model->meshes_count) continue;

        auto& gltfMesh = m_model->meshes[gltfNode.mesh];
        
        for (uint32_t pIdx = 0; pIdx < gltfMesh.primitives_count; ++pIdx) {
            auto& primitive = gltfMesh.primitives[pIdx];
            SubMesh sub;
            
            sub.nodeIndex = nIdx;
            sub.nodeName = ModelUtils::toString(gltfNode.name);
            sub.name = ModelUtils::toString(gltfMesh.name);

            if (sub.name.empty()) sub.name = fmt::format("mesh_{}", gltfNode.mesh);
            if (gltfMesh.primitives_count > 1) sub.name += fmt::format("_part_{}", pIdx);

            if (primitive.material >= 0 && static_cast<uint32_t>(primitive.material) < m_model->materials_count) {
                auto& mat = m_model->materials[primitive.material];

                int texIdx = mat.pbr_metallic_roughness.base_color_texture.index;

                if (texIdx >= 0 && static_cast<uint32_t>(texIdx) < m_model->textures_count) {
                    sub.embeddedImageIndex = m_model->textures[texIdx].source;
                }
            }

            auto position = findAttribute(primitive, GLB_ATTRIBUTE_POSITION);
            if (!position) continue;

            extractFloatData(position->value, 3, false, [&](const std::vector<float>& vals) {
                sub.basePositions.push_back({vals[0], vals[1], vals[2]});
            });

            if (auto uv = findAttribute(primitive, GLB_ATTRIBUTE_UV)) {
                extractFloatData(uv->value, 2, false, [&](const std::vector<float>& vals) {
                    sub.uvs.push_back({vals[0], vals[1]}); 
                });
            } else {
                sub.uvs.assign(sub.basePositions.size(), {0,0});
            }

            if (auto weights = findAttribute(primitive, GLB_ATTRIBUTE_WEIGHTS)) {
                extractFloatData(weights->value, 4, true, [&](const std::vector<float>& vals) {
                    sub.weights.push_back({vals[0], vals[1], vals[2], vals[3]});
                });
            }
            
            if (auto joints = findAttribute(primitive, GLB_ATTRIBUTE_JOINTS)) {
                extractFloatData(joints->value, 4, false, [&](const std::vector<float>& vals) {
                    sub.joints.push_back({vals[0], vals[1], vals[2], vals[3]});
                });
            }

            if (primitive.indices >= 0) {
                auto& accessor = m_model->accessors[primitive.indices];
                auto& bufferView = m_model->buffer_views[accessor.buffer_view];
                auto& buffer = m_model->buffers[bufferView.buffer];
                const unsigned char* data = buffer.data.data + bufferView.byte_offset + accessor.byte_offset;
                
                size_t componentSize = 0;
                switch (accessor.component_type) {
                    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                        componentSize = 1;
                        break;
                    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                        componentSize = 2;
                        break;
                    case TG3_COMPONENT_TYPE_UNSIGNED_INT:
                        componentSize = 4;
                        break;
                }

                if (componentSize > 0) {
                    const size_t stride = bufferView.byte_stride ? bufferView.byte_stride : componentSize;
                    sub.indices.reserve(accessor.count);

                    for (size_t i = 0; i < accessor.count; i++) {
                        const auto* bytes = data + i * stride;
                        uint32_t index = 0;
                        for (size_t byte = 0; byte < componentSize; byte++) {
                            index |= static_cast<uint32_t>(bytes[byte]) << (byte * 8);
                        }
                        sub.indices.push_back(static_cast<unsigned short>(index));
                    }
                }
            }

            sub.renderVertices.resize(sub.basePositions.size());
            m_subMeshes.push_back(sub);
        }
    }

    if (m_model->skins_count > 0) {
        auto& skin = m_model->skins[0];
        extractFloatData(skin.inverse_bind_matrices, 16, false, [this](const std::vector<float>& vals) {
            Mat4 mat;
            
            for (int i = 0; i < 16; i++) {
                mat.m[i] = vals[i];
            }
            
            m_inverseBindMatrices.push_back(mat);
        });
    }
}

void SkeletonPlayer::buildBoneNodes() {
    m_boneNodes.clear();
    
    std::unordered_set<int> skeletonNodes;
    if (m_model->skins_count > 0) {
        const auto& skin = m_model->skins[0];
        for (uint32_t i = 0; i < skin.joints_count; i++) {
            skeletonNodes.insert(skin.joints[i]);
        }
    }

    for (uint32_t i = 0; i < m_model->nodes_count; i++) {
        auto& gltfNode = m_model->nodes[i];
        
        bool isImportant = skeletonNodes.count(i) || gltfNode.children_count > 0;
        
        if (isImportant) {
            const std::string nodeName = ModelUtils::toString(gltfNode.name);
            auto bone = BoneNode::create(i, nodeName.empty() ? fmt::format("bone_{}", i) : nodeName);
            
            Vec3 t = Vec3::zero();
            Vec3 s = Vec3::one();
            Quat r = Quat::identity();

            t = {
                static_cast<float>(gltfNode.translation[0]),
                static_cast<float>(gltfNode.translation[1]),
                static_cast<float>(gltfNode.translation[2])
            };
            r = {
                static_cast<float>(gltfNode.rotation[0]),
                static_cast<float>(gltfNode.rotation[1]),
                static_cast<float>(gltfNode.rotation[2]),
                static_cast<float>(gltfNode.rotation[3])
            };
            s = {
                static_cast<float>(gltfNode.scale[0]),
                static_cast<float>(gltfNode.scale[1]),
                static_cast<float>(gltfNode.scale[2])
            };
            
            bone->updateFromGLTF(t, r, s);
            m_boneNodes[i] = bone;
        }
    }

    for (uint32_t i = 0; i < m_model->nodes_count; i++) {
        if (!m_boneNodes.count(i)) continue;

        for (uint32_t childIndex = 0; childIndex < m_model->nodes[i].children_count; childIndex++) {
            const int child = m_model->nodes[i].children[childIndex];
            if (!m_boneNodes.count(child)) continue;
            
            m_boneNodes[i]->addChild(m_boneNodes[child]);
        }
    }

    int sceneIndex = m_model->default_scene > -1 ? m_model->default_scene : 0;

    if (static_cast<uint32_t>(sceneIndex) >= m_model->scenes_count) return;

    const auto& scene = m_model->scenes[sceneIndex];
    for (uint32_t i = 0; i < scene.nodes_count; i++) {
        auto rootIdx = scene.nodes[i];
        if (!m_boneNodes.count(rootIdx)) continue;

        this->addChild(m_boneNodes[rootIdx]);
    }
}

void SkeletonPlayer::extractAnimations() {
    m_animations.clear();
    if (m_model->animations_count == 0) return;

    for (uint32_t animIndex = 0; animIndex < m_model->animations_count; animIndex++) {
        const auto& gltfAnim = m_model->animations[animIndex];
        auto animationName = ModelUtils::toString(gltfAnim.name);
        Animation anim{
            .name = animationName.empty() ? fmt::format("anim_{}", m_animations.size()) : animationName
        };
        
        for (uint32_t channelIndex = 0; channelIndex < gltfAnim.channels_count; channelIndex++) {
            const auto& channel = gltfAnim.channels[channelIndex];
            auto& sampler = gltfAnim.samplers[channel.sampler];

            Track track{
                .targetNode = channel.target.node,
                .path = ModelUtils::toString(channel.target.path),
                .interpolation = ModelUtils::toString(sampler.interpolation)
            };

            extractFloatData(sampler.input, 1, false, [&](const std::vector<float>& vals) {
                track.times.push_back(vals[0]);
                if (vals[0] > anim.duration) anim.duration = vals[0];
            });

            int numComp = (track.path == GLB_ANIM_PATH_ROTATION) ? 4 : 3;
            extractFloatData(sampler.output, numComp, false, [&](const std::vector<float>& vals) {
                track.values.push_back(vals);
            });
            anim.tracks.push_back(track);
        }
        m_animations[anim.name] = anim;
    }
}

void SkeletonPlayer::applyAnimations(float time) {
    for (auto& track : m_tracks) {
        if (track.times.empty() || track.values.empty()) continue;
        
        int frame = 0;
        int nextFrame = 0;
        float factor = 0.f;

        if (time >= track.times.back()) {
            frame = track.times.size() - 1;
            nextFrame = frame;
            factor = 0.f;
        } else {
            for (size_t i = 0; i < track.times.size() - 1; i++) {
                if (time < track.times[i] || time >= track.times[i + 1]) continue;

                frame = i; 
                nextFrame = i + 1;
                float t0 = track.times[frame];
                float t1 = track.times[nextFrame];
                factor = (time - t0) / (t1 - t0);
                break; 
            }
        }

        int index0 = frame;
        int index1 = nextFrame;
        
        if (track.interpolation == GLB_ANIM_INTERPOLATION_CUBICSPLINE) {
            index0 = frame * 3 + 1;
            index1 = nextFrame * 3 + 1;
        }

        if (index1 >= track.values.size()) continue;

        auto& value0 = track.values[index0]; 
        auto& value1 = track.values[index1];
        auto bone = m_boneNodes[track.targetNode];

        if (track.path == GLB_ANIM_PATH_TRANSLATION) {
            Vec3 t = Vec3::lerp({
                    value0[0], 
                    value0[1], 
                    value0[2]
                }, {
                    value1[0], 
                    value1[1], 
                    value1[2]
                }, factor
            );
            bone->updateFromGLTF(t, std::nullopt, std::nullopt);
        } 
        else if (track.path == GLB_ANIM_PATH_SCALE) {
            Vec3 s = Vec3::lerp({
                    value0[0], 
                    value0[1], 
                    value0[2]
                }, {
                    value1[0], 
                    value1[1], 
                    value1[2]
                }, factor
            );
            bone->updateFromGLTF(std::nullopt, std::nullopt, s);
        } 
        else if (track.path == GLB_ANIM_PATH_ROTATION) {
            Quat q = Quat::nlerp({
                    value0[0], 
                    value0[1], 
                    value0[2], 
                    value0[3]
                }, {
                    value1[0], 
                    value1[1], 
                    value1[2], 
                    value1[3]
                }, factor);
            bone->updateFromGLTF(std::nullopt, q, std::nullopt);
        }
    }
}

void SkeletonPlayer::recalcGlobalMatrices(int nodeIndex, const Mat4& parentGlobal) {
    if (m_boneNodes.find(nodeIndex) == m_boneNodes.end()) {
        if (nodeIndex >= 0 && static_cast<uint32_t>(nodeIndex) < m_model->nodes_count) {
            const auto& node = m_model->nodes[nodeIndex];

            for (uint32_t i = 0; i < node.children_count; i++) {
                recalcGlobalMatrices(node.children[i], parentGlobal);
            }
        }
        
        return;
    }

    auto bone = m_boneNodes[nodeIndex];
    
    bone->setGlobalMatrix(parentGlobal * Mat4::fromTRS(bone->getLocalTranslation(), bone->getLocalRotation(), bone->getLocalScale()));

    if (m_model->skins_count > 0) {
        auto& skin = m_model->skins[0];

        for (uint32_t i = 0; i < skin.joints_count; i++) {
            if (skin.joints[i] != nodeIndex) continue;

            m_globalJointMatrices[i] = bone->getGlobalMatrix() * m_inverseBindMatrices[i];
            break;
        }
    }

    const auto& node = m_model->nodes[nodeIndex];
    for (uint32_t i = 0; i < node.children_count; i++) {
        recalcGlobalMatrices(node.children[i], bone->getGlobalMatrix());
    }
}

void SkeletonPlayer::applySkinning() {
    for (auto& sub : m_subMeshes) {
        
        Mat4 nodeGlobalMat;
        if (sub.nodeIndex >= 0 && m_boneNodes.count(sub.nodeIndex)) {
            nodeGlobalMat = m_boneNodes[sub.nodeIndex]->getGlobalMatrix();
        }

        CCSpriteFrame* frame = nullptr;
        if (m_customFrames.count(sub.nodeName)) {
            frame = m_customFrames[sub.nodeName];
        }

        for (size_t i = 0; i < sub.basePositions.size(); i++) {
            Vec3 base = sub.basePositions[i];
            Vec3 finalPos = Vec3::zero();

            if (!sub.weights.empty() && !sub.joints.empty()) {
                
                Vec4 w = sub.weights[i];
                Vec4 j = sub.joints[i];

                for (int k = 0; k < 4; k++) {
                    float weight =
                        (k == 0) ? w.x :
                        (k == 1) ? w.y :
                        (k == 2) ? w.z : w.w;
                    int jointIdx = static_cast<int>(
                        (k == 0) ? j.x : 
                        (k == 1) ? j.y :
                        (k == 2) ? j.z : j.w
                    );

                    if (weight > 0.f && jointIdx >= 0 && jointIdx < m_globalJointMatrices.size()) {
                        Vec3 transformed = m_globalJointMatrices[jointIdx] * base;
                        finalPos.x += transformed.x * weight; 
                        finalPos.y += transformed.y * weight; 
                        finalPos.z += transformed.z * weight;
                    }
                }
            } else {
                finalPos = nodeGlobalMat * base;
            }

            sub.renderVertices[i].pos = finalPos * GLB_PIXEL_SCALE;

            if (frame) {
                auto rect = frame->getRectInPixels();
                auto texSize = frame->getTexture()->getContentSizeInPixels();
                auto origUV = sub.uvs[i];

                if (!frame->isRotated()) {
                    sub.renderVertices[i].uv.x = (rect.origin.x + origUV.x * rect.size.width) / texSize.width;
                    sub.renderVertices[i].uv.y = (rect.origin.y + (1.0f - origUV.y) * rect.size.height) / texSize.height;
                } else {
                    sub.renderVertices[i].uv.x = (rect.origin.x + (1.0f - origUV.y) * rect.size.width) / texSize.width;
                    sub.renderVertices[i].uv.y = (rect.origin.y + origUV.x * rect.size.height) / texSize.height;
                }
            } else {
                sub.renderVertices[i].uv = sub.uvs[i];
            }
            
            sub.renderVertices[i].color = {
                this->getColor().r / 255.f, 
                this->getColor().g / 255.f, 
                this->getColor().b / 255.f, 
                this->getOpacity() / 255.f
            };
        }
    }
}

void SkeletonPlayer::extractFloatData(int accessorIndex, int expectedComponents, bool isWeight, std::function<void(const std::vector<float>&)> callback) {
    if (accessorIndex < 0) return;

    auto& accessor = m_model->accessors[accessorIndex];

    if (accessor.buffer_view < 0 || static_cast<uint32_t>(accessor.buffer_view) >= m_model->buffer_views_count) return;

    auto& bufferView = m_model->buffer_views[accessor.buffer_view];
    auto& buffer = m_model->buffers[bufferView.buffer];
    
    const unsigned char* data = buffer.data.data + bufferView.byte_offset + accessor.byte_offset;
    
    size_t compSize = 0;
    switch (accessor.component_type) {
        case TG3_COMPONENT_TYPE_FLOAT:
            compSize = sizeof(uint32_t);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            compSize = sizeof(uint16_t);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
            compSize = sizeof(uint8_t);
            break;
        default:
            return;
    }
    
    const size_t stride = bufferView.byte_stride ? bufferView.byte_stride : (expectedComponents * compSize);

    float normalizeFactor = 1.0f;
    if (accessor.normalized || isWeight) {
        if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) 
            normalizeFactor = 65535.0f;
        if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE)
            normalizeFactor = 255.0f;
    }

    const auto readLittleEndian = [](const unsigned char* bytes, size_t byteCount) -> uint32_t {
        uint32_t value = 0;
        for (size_t byte = 0; byte < byteCount; byte++) {
            value |= static_cast<uint32_t>(bytes[byte]) << (byte * 8);
        }
        return value;
    };

    for (size_t i = 0; i < accessor.count; ++i) {
        std::vector<float> vals(expectedComponents, 0.0f);
        auto element = data + i * stride;
        
        for (int c = 0; c < expectedComponents; ++c) {
            auto* component = element + c * compSize;
            switch (accessor.component_type) {
                case TG3_COMPONENT_TYPE_FLOAT:
                    vals[c] = std::bit_cast<float>(readLittleEndian(component, compSize));
                    break;
                case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                    vals[c] = static_cast<uint16_t>(readLittleEndian(component, compSize)) / normalizeFactor;
                    break;
                case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                    vals[c] = readLittleEndian(component, compSize) / normalizeFactor;
                    break;
            }
        }
        callback(vals);
    }
}

std::vector<BoneNode*> SkeletonPlayer::getBoneNodes() {
    std::vector<BoneNode*> ret;
    for (auto& [id, bone] : m_boneNodes) {
        ret.push_back(bone);
    }
    return ret;
}

std::vector<float> SkeletonPlayer::sampleTrack(const Track& track, float time) {
    if (track.times.empty()) return {};
    
    int frame = 0;
    int nextFrame = 0;
    float factor = 0.f;

    if (time >= track.times.back()) {
        frame = track.times.size() - 1;
        nextFrame = frame;
    } else {
        for (size_t i = 0; i < track.times.size() - 1; i++) {
            if (time < track.times[i] || time >= track.times[i + 1]) continue;

            frame = i;
            nextFrame = i + 1;
            factor = (time - track.times[frame]) / (track.times[nextFrame] - track.times[frame]);

            break;
        }
    }

    int index0 = (track.interpolation == GLB_ANIM_INTERPOLATION_CUBICSPLINE) ? frame * 3 + 1 : frame;
    int index1 = (track.interpolation == GLB_ANIM_INTERPOLATION_CUBICSPLINE) ? nextFrame * 3 + 1 : nextFrame;
    
    std::vector<float> result(track.values[index0].size());

    for (size_t i = 0; i < result.size(); i++) {
        result[i] = track.values[index0][i] + (track.values[index1][i] - track.values[index0][i]) * factor;
    }
    return result;
}

void SkeletonPlayer::playAnimation(const std::string& name, float blendTime) {
    if (m_animations.find(name) == m_animations.end() || m_currentAnimName == name) return;

    if (blendTime <= 0.0f) {
        m_currentAnimName = name;
        m_time = 0.0f;
        m_blendingWeight = 1.0f;
    } else {
        m_prevAnimName = m_currentAnimName;
        m_prevAnimTime = m_time;
        m_currentAnimName = name;
        m_time = 0.0f;
        m_blendingTimer = 0.0f;
        m_blendingDuration = blendTime;
        m_blendingWeight = 0.0f; 
    }
}

std::vector<std::string> SkeletonPlayer::getAnimationNames() {
    std::vector<std::string> ret{};
    for (const auto& anim : m_animations) {
        ret.push_back(anim.first);
    }

    return ret;
}

void SkeletonPlayer::setAnimSpeedMultiplier(float multiplier) {
   m_animSpeedModifier = multiplier;
}

const tg3_str_int_pair* SkeletonPlayer::findAttribute(const tg3_primitive& primitive, const char* name) {
    for (uint32_t i = 0; i < primitive.attributes_count; i++) {
        if (tg3_str_equals_cstr(primitive.attributes[i].key, name)) {
            return &primitive.attributes[i];
        }
    }
    return nullptr;
}