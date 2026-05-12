#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <SkeletonPlayer.hpp>

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
    if (m_customTextures.count(meshName)){
        CC_SAFE_RELEASE(m_customTextures[meshName]);
    }

    if (m_customFrames.count(meshName)) {
        CC_SAFE_RELEASE(m_customFrames[meshName]);
        m_customFrames.erase(meshName);
    }

    if (tex){
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

void SkeletonPlayer::loadFromGLTF(const std::shared_ptr<tinygltf::Model>& model) {
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
            Vec3 finalT = bone->getLocalTranslation();
            Quat finalR = bone->getLocalRotation();
            Vec3 finalS = bone->getLocalScale();

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
                Vec3 transform1 = finalT;
                Vec3 transform2 = finalT;
                Quat rotation1 = finalR;
                Quat rotation2 = finalR;
                Vec3 scale1 = finalS;
                Vec3 scale2 = finalS;

                getAnimTRS(m_animations[m_prevAnimName], m_prevAnimTime, transform1, rotation1, scale1);
                getAnimTRS(currentAnim, m_time, transform2, rotation2, scale2);

                finalT = Vec3::lerp(transform1, transform2, m_blendingWeight);
                finalR = Quat::nlerp(rotation1, rotation2, m_blendingWeight);
                finalS = Vec3::lerp(scale1, scale2, m_blendingWeight);
            } else {
                getAnimTRS(currentAnim, m_time, finalT, finalR, finalS);
            }

            bone->updateFromGLTF(finalT, finalR, finalS);
        }
    }

    for (auto& [id, bone] : m_boneNodes) {
        bone->syncFromCCNode();
    }

    int sceneIndex = m_model->defaultScene > -1 ? m_model->defaultScene : 0;
    for (int root : m_model->scenes[sceneIndex].nodes) {
        recalcGlobalMatrices(root, Mat4());
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

    ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);

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
    for (size_t i = 0; i < m_model->images.size(); ++i) {
        auto& img = m_model->images[i];

        if (img.image.empty()) continue;

        CCTexture2D* tex = new CCTexture2D();

        tex->initWithData(
            img.image.data(),
            (img.component == 3) ? kCCTexture2DPixelFormat_RGB888 : kCCTexture2DPixelFormat_RGBA8888,
            img.width,
            img.height,
            CCSize(
                img.width,
                img.height
            )
        );

        m_embeddedTextures[i] = tex; 
    }
}

void SkeletonPlayer::extractMeshes() {
    m_subMeshes.clear();
    
    for (size_t nIdx = 0; nIdx < m_model->nodes.size(); ++nIdx) {
        auto& gltfNode = m_model->nodes[nIdx];
        
        if (gltfNode.mesh < 0 || gltfNode.mesh >= m_model->meshes.size()) continue;

        auto& gltfMesh = m_model->meshes[gltfNode.mesh];
        
        for (size_t pIdx = 0; pIdx < gltfMesh.primitives.size(); ++pIdx) {
            auto& primitive = gltfMesh.primitives[pIdx];
            SubMesh sub;
            
            sub.nodeIndex = nIdx;
            sub.nodeName = gltfNode.name;
            sub.name = gltfMesh.name;
            if (sub.name.empty()) sub.name = fmt::format("mesh_{}", gltfNode.mesh);
            if (gltfMesh.primitives.size() > 1) sub.name += fmt::format("_part_{}", pIdx);

            if (primitive.material >= 0) {
                auto& mat = m_model->materials[primitive.material];
                int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
                if (texIdx >= 0 && texIdx < m_model->textures.size()) sub.embeddedImageIndex = m_model->textures[texIdx].source;
            }

            extractFloatData(primitive.attributes[GLB_ATTRIBUTE_POSITION], 3, false, [&](const std::vector<float>& vals) {
                sub.basePositions.push_back({vals[0], vals[1], vals[2]});
            });

            if (primitive.attributes.count(GLB_ATTRIBUTE_UV)) {
                extractFloatData(primitive.attributes[GLB_ATTRIBUTE_UV], 2, false, [&](const std::vector<float>& vals) {
                    sub.uvs.push_back({vals[0], vals[1]}); 
                });
            } else {
                sub.uvs.assign(sub.basePositions.size(), {0,0});
            }

            if (primitive.attributes.count(GLB_ATTRIBUTE_WEIGHTS)) {
                extractFloatData(primitive.attributes[GLB_ATTRIBUTE_WEIGHTS], 4, true, [&](const std::vector<float>& vals) {
                    sub.weights.push_back({vals[0], vals[1], vals[2], vals[3]});
                });
            }
            
            if (primitive.attributes.count(GLB_ATTRIBUTE_JOINTS)) {
                extractFloatData(primitive.attributes[GLB_ATTRIBUTE_JOINTS], 4, false, [&](const std::vector<float>& vals) {
                    sub.joints.push_back({vals[0], vals[1], vals[2], vals[3]});
                });
            }

            if (primitive.indices >= 0) {
                auto& accessor = m_model->accessors[primitive.indices];
                auto& bufferView = m_model->bufferViews[accessor.bufferView];
                auto& buffer = m_model->buffers[bufferView.buffer];
                const unsigned char* data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
                
                int compSize = (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) ? 4 :
                                (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? 2 : 1;
                int stride = bufferView.byteStride ? bufferView.byteStride : compSize;

                for (size_t i = 0; i < accessor.count; ++i) {
                    const unsigned char* ptr = data + (i * stride);
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        unsigned short idx; memcpy(&idx, ptr, 2); sub.indices.push_back(idx);
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        unsigned int idx; memcpy(&idx, ptr, 4); sub.indices.push_back((unsigned short)idx);
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        unsigned char idx; memcpy(&idx, ptr, 1); sub.indices.push_back((unsigned short)idx);
                    }
                }
            }

            sub.renderVertices.resize(sub.basePositions.size());
            m_subMeshes.push_back(sub);
        }
    }

    if (!m_model->skins.empty()) {
        auto& skin = m_model->skins[0];
        extractFloatData(skin.inverseBindMatrices, 16, false, [this](const std::vector<float>& vals) {
            Mat4 mat;
            
            for(int i = 0; i < 16; i++) {
                mat.m[i] = vals[i];
            }
            
            m_inverseBindMatrices.push_back(mat);
        });
    }
}

void SkeletonPlayer::buildBoneNodes() {
    m_boneNodes.clear();
    
    std::unordered_set<int> skeletonNodes;
    if (!m_model->skins.empty()) {
        for (int jointIdx : m_model->skins[0].joints) {
            skeletonNodes.insert(jointIdx);
        }
    }

    for (size_t i = 0; i < m_model->nodes.size(); ++i) {
        auto& gltfNode = m_model->nodes[i];
        
        bool isImportant = skeletonNodes.count(i) || !gltfNode.children.empty();
        
        if (isImportant) {
            auto bone = BoneNode::create(i, gltfNode.name.empty() ? fmt::format("bone_{}", i) : gltfNode.name);
            
            Vec3 t = Vec3::zero();
            Vec3 s = Vec3::one();
            Quat r = Quat::identity();

            if (gltfNode.translation.size() == 3){
                t = {
                    static_cast<float>(gltfNode.translation[0]),
                    static_cast<float>(gltfNode.translation[1]),
                    static_cast<float>(gltfNode.translation[2])
                };
            }
            if (gltfNode.rotation.size() == 4){
                r = {
                    static_cast<float>(gltfNode.rotation[0]),
                    static_cast<float>(gltfNode.rotation[1]),
                    static_cast<float>(gltfNode.rotation[2]),
                    static_cast<float>(gltfNode.rotation[3])
                };
            }
            if (gltfNode.scale.size() == 3){
                s = {
                    static_cast<float>(gltfNode.scale[0]),
                    static_cast<float>(gltfNode.scale[1]),
                    static_cast<float>(gltfNode.scale[2])
                };
            }
            
            bone->updateFromGLTF(t, r, s);
            m_boneNodes[i] = bone;
        }
    }

    for (size_t i = 0; i < m_model->nodes.size(); ++i) {
        if (!m_boneNodes.count(i)) continue;

        for (int child : m_model->nodes[i].children) {
            if (!m_boneNodes.count(child)) continue;
            
            m_boneNodes[i]->addChild(m_boneNodes[child]);
        }
    }

    int sceneIndex = m_model->defaultScene > -1 ? m_model->defaultScene : 0;
    for (int rootIdx : m_model->scenes[sceneIndex].nodes) {
        if (!m_boneNodes.count(rootIdx)) continue;

        this->addChild(m_boneNodes[rootIdx]);
    }
}

void SkeletonPlayer::extractAnimations() {
    m_animations.clear();
    if (m_model->animations.empty()) return;

    for (auto& gltfAnim : m_model->animations) {
        Animation anim;
        anim.name = gltfAnim.name.empty() ? fmt::format("anim_{}", m_animations.size()) : gltfAnim.name;
        
        for (auto& channel : gltfAnim.channels) {
            auto& sampler = gltfAnim.samplers[channel.sampler];
            Track track;
            track.targetNode = channel.target_node;
            track.path = channel.target_path;
            track.interpolation = sampler.interpolation;

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
            for (size_t i = 0; i < track.times.size() - 1; ++i) {
                if (time < track.times[i] || time >= track.times[i+1]) continue;

                frame = i; 
                nextFrame = i + 1;
                float t0 = track.times[frame];
                float t1 = track.times[nextFrame];
                factor = (time - t0) / (t1 - t0);
                break; 
            }
        }

        int vIdx0 = frame;
        int vIdx1 = nextFrame;
        
        if (track.interpolation == GLB_ANIM_INTERPOLATION_CUBICSPLINE) {
            vIdx0 = frame * 3 + 1;
            vIdx1 = nextFrame * 3 + 1;
        }

        if (vIdx1 >= track.values.size()) continue;

        auto& v0 = track.values[vIdx0]; 
        auto& v1 = track.values[vIdx1];
        auto bone = m_boneNodes[track.targetNode];

        if (track.path == GLB_ANIM_PATH_TRANSLATION) {
            Vec3 t = Vec3::lerp({
                    v0[0], 
                    v0[1], 
                    v0[2]
                }, {
                    v1[0], 
                    v1[1], 
                    v1[2]
                }, factor
            );
            bone->updateFromGLTF(t, std::nullopt, std::nullopt);
        } 
        else if (track.path == GLB_ANIM_PATH_SCALE) {
            Vec3 s = Vec3::lerp({
                    v0[0], 
                    v0[1], 
                    v0[2]
                }, {
                    v1[0], 
                    v1[1], 
                    v1[2]
                }, factor
            );
            bone->updateFromGLTF(std::nullopt, std::nullopt, s);
        } 
        else if (track.path == GLB_ANIM_PATH_ROTATION) {
            Quat q = Quat::nlerp({
                    v0[0], 
                    v0[1], 
                    v0[2], 
                    v0[3]
                }, {
                    v1[0], 
                    v1[1], 
                    v1[2], 
                    v1[3]
                }, factor);
            bone->updateFromGLTF(std::nullopt, q, std::nullopt);
        }
    }
}

void SkeletonPlayer::recalcGlobalMatrices(int nodeIndex, const Mat4& parentGlobal) {
    if (m_boneNodes.find(nodeIndex) == m_boneNodes.end()) {
        for (int child : m_model->nodes[nodeIndex].children) {
            recalcGlobalMatrices(child, parentGlobal);
        }
        return;
    }

    auto bone = m_boneNodes[nodeIndex];
    
    bone->setGlobalMatrix(parentGlobal * Mat4::fromTRS(bone->getLocalTranslation(), bone->getLocalRotation(), bone->getLocalScale()));

    if (!m_model->skins.empty()) {
        auto& skin = m_model->skins[0];

        for (size_t i = 0; i < skin.joints.size(); ++i) {
            if (skin.joints[i] != nodeIndex) continue;

            m_globalJointMatrices[i] = bone->getGlobalMatrix() * m_inverseBindMatrices[i];
            break;
        }
    }

    for (int child : m_model->nodes[nodeIndex].children) {
        recalcGlobalMatrices(child, bone->getGlobalMatrix());
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

        for (size_t i = 0; i < sub.basePositions.size(); ++i) {
            Vec3 base = sub.basePositions[i];
            Vec3 finalPos = Vec3::zero();

            if (!sub.weights.empty() && !sub.joints.empty()) {
                
                Vec4 w = sub.weights[i];
                Vec4 j = sub.joints[i];

                for (int k = 0; k < 4; ++k) {
                    float weight = (k==0)?w.x : (k==1)?w.y : (k==2)?w.z : w.w;
                    int jointIdx = static_cast<int>((k==0)?j.x : (k==1)?j.y : (k==2)?j.z : j.w);

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
                CCRect rect = frame->getRectInPixels();
                CCSize texSize = frame->getTexture()->getContentSizeInPixels();
                Vec2 origUV = sub.uvs[i];

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
    auto& bufferView = m_model->bufferViews[accessor.bufferView];
    auto& buffer = m_model->buffers[bufferView.buffer];
    
    const unsigned char* data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
    
    int compSize = 4;
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) compSize = 2;
    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) compSize = 1;
    
    int stride = bufferView.byteStride ? bufferView.byteStride : (expectedComponents * compSize);

    float normalizeFactor = 1.0f;
    if (accessor.normalized || isWeight) {
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) normalizeFactor = 65535.0f;
        if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) normalizeFactor = 255.0f;
    }

    for (size_t i = 0; i < accessor.count; ++i) {
        std::vector<float> vals(expectedComponents, 0.0f);
        const unsigned char* ptr = data + (i * stride);
        
        for (int c = 0; c < expectedComponents; ++c) {
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float v;
                memcpy(&v, ptr + c * 4, 4);
                vals[c] = v;
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                unsigned short v;
                memcpy(&v, ptr + c * 2, 2);
                vals[c] = v / normalizeFactor;
            } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                unsigned char v;
                memcpy(&v, ptr + c, 1);
                vals[c] = v / normalizeFactor;
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
        for (size_t i = 0; i < track.times.size() - 1; ++i) {
            if (time < track.times[i] || time >= track.times[i + 1]) continue;

            frame = i;
            nextFrame = i + 1;
            factor = (time - track.times[frame]) / (track.times[nextFrame] - track.times[frame]);

            break;
        }
    }

    int vIdx0 = (track.interpolation == GLB_ANIM_INTERPOLATION_CUBICSPLINE) ? frame * 3 + 1 : frame;
    int vIdx1 = (track.interpolation == GLB_ANIM_INTERPOLATION_CUBICSPLINE) ? nextFrame * 3 + 1 : nextFrame;
    
    std::vector<float> result(track.values[vIdx0].size());
    for(size_t i = 0; i < result.size(); ++i) {
        result[i] = track.values[vIdx0][i] + (track.values[vIdx1][i] - track.values[vIdx0][i]) * factor;
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

 std::vector<std::string> SkeletonPlayer::getAnimationNames(){
    std::vector<std::string> ret{};
    for (const auto& anim : m_animations)
    {
        ret.push_back(anim.first);
    }
    
    return ret;
 }

 void SkeletonPlayer::setAnimSpeedMultiplier(float multiplier){
    m_animSpeedModifier = multiplier;
 }