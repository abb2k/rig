#pragma once

#include <Geode/Geode.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include "Types.hpp"
#include "BoneNode.hpp"

using namespace geode::prelude;

class SkeletonPlayer : public CCNode, public CCRGBAProtocol {
protected:
    tinygltf::Model m_model;
    bool m_loaded = false;
    bool m_paused = false;
    float m_time = 0.0f;
    float m_maxAnimTime = 0.0f;

    std::map<int, BoneNode*> m_boneNodes;

    std::vector<SubMesh> m_subMeshes;

    std::vector<Mat4> m_inverseBindMatrices;
    std::vector<Mat4> m_globalJointMatrices;

    std::map<int, CCTexture2D*> m_embeddedTextures;
    std::map<std::string, CCTexture2D*> m_customTextures;

    std::vector<Track> m_tracks;

    ccColor3B m_color = {255, 255, 255};
    GLubyte m_opacity = 255;
    bool m_opacityModifyRGB;
    bool m_cascadeColorEnabled;
    bool m_cascadeOpacityEnabled;

public:
    static SkeletonPlayer* create() {
        auto ret = new SkeletonPlayer();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        this->setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor));
        this->scheduleUpdate();
        return true;
    }

    ~SkeletonPlayer() {
        for (auto& pair : m_embeddedTextures) CC_SAFE_RELEASE(pair.second);
        for (auto& pair : m_customTextures) CC_SAFE_RELEASE(pair.second);
    }

    virtual void setColor(const ccColor3B& color) override{
        m_color = color;
    }

    virtual const ccColor3B& getColor() override{
        return m_color;
    }

    virtual const ccColor3B& getDisplayedColor() override{
        return m_color;
    }

    virtual GLubyte getDisplayedOpacity() override{
        return m_opacity;
    }

    virtual GLubyte getOpacity(void) override{
        return m_opacity;
    }

    virtual void setOpacity(GLubyte opacity) override{
        m_opacity = opacity;
    }

    virtual void setOpacityModifyRGB(bool bValue) override{
        m_opacityModifyRGB = bValue;
    }

    virtual bool isOpacityModifyRGB() override{
        return m_opacityModifyRGB;
    }

    virtual bool isCascadeColorEnabled() override{
        return m_cascadeColorEnabled;
    }
    virtual void setCascadeColorEnabled(bool cascadeColorEnabled) override{
        m_cascadeColorEnabled = cascadeColorEnabled;
    }

    virtual void updateDisplayedColor(const ccColor3B& color) override{
        m_color = color;
    }

    virtual bool isCascadeOpacityEnabled() override{
        return m_cascadeOpacityEnabled;
    }
    virtual void setCascadeOpacityEnabled(bool cascadeOpacityEnabled) override{
        m_cascadeOpacityEnabled = cascadeOpacityEnabled;
    }
 
    virtual void updateDisplayedOpacity(GLubyte opacity) override{
        m_opacity = opacity;
    }

    void togglePause() { m_paused = !m_paused; }

    void setTextureForMesh(const std::string& meshName, CCTexture2D* tex) {
        if (m_customTextures.count(meshName)) CC_SAFE_RELEASE(m_customTextures[meshName]);
        if (tex) tex->retain();
        m_customTextures[meshName] = tex;
    }

    void loadFromGLTF(const tinygltf::Model& model) {
        m_model = model;
        m_loaded = false;
        
        // BUG FIX: Completely reset the time state when loading a new file!
        m_time = 0.0f;
        m_maxAnimTime = 0.0f;
        m_tracks.clear(); 

        loadEmbeddedTextures();
        buildBoneNodes();
        extractMeshes();
        extractAnimations();

        m_globalJointMatrices.resize(m_inverseBindMatrices.size());
        m_loaded = true;
    }

    void update(float dt) override {
        if (!m_loaded) return;

        if (!m_paused) {
            m_time += dt;
            if (m_maxAnimTime > 0 && m_time > m_maxAnimTime) {
                m_time = fmod(m_time, m_maxAnimTime);
            }
            applyAnimations(m_time);
        }

        for (auto& pair : m_boneNodes) pair.second->syncFromCCNode();

        int sceneIndex = m_model.defaultScene > -1 ? m_model.defaultScene : 0;
        for (int root : m_model.scenes[sceneIndex].nodes) {
            recalcGlobalMatrices(root, Mat4());
        }

        applySkinning();
    }

    void draw() override {
        if (!m_loaded) return;
        auto shader = this->getShaderProgram();
        if (!shader) return;

        CC_NODE_DRAW_SETUP();
        ccGLEnableVertexAttribs(kCCVertexAttribFlag_PosColorTex);
        shader->use();
        shader->setUniformsForBuiltins();

        // --- THE FIX: Force Normal Alpha Blending! ---
        // Overrides Cocos2d's default additive/pre-multiplied blending
        ccGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        GLboolean wasCullEnabled;
        glGetBooleanv(GL_CULL_FACE, &wasCullEnabled);
        glDisable(GL_CULL_FACE);

        // Simple Back-to-Front Sorting
        std::vector<std::pair<int, float>> depthSortedMeshes;
        for (size_t i = 0; i < m_subMeshes.size(); i++) {
            float avgZ = 0.0f;
            if (!m_subMeshes[i].renderVertices.empty()) {
                for (auto& v : m_subMeshes[i].renderVertices) {
                    avgZ += v.pos.z; 
                }
                avgZ /= m_subMeshes[i].renderVertices.size();
            }
            depthSortedMeshes.push_back({(int)i, avgZ});
        }

        std::sort(depthSortedMeshes.begin(), depthSortedMeshes.end(), [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
            return a.second < b.second; 
        });

        // Draw Meshes
        for (auto& pair : depthSortedMeshes) {
            auto& sub = m_subMeshes[pair.first];
            if (sub.indices.empty() || sub.renderVertices.empty()) continue;

            glVertexAttribPointer(kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), &sub.renderVertices[0].pos);
            glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), &sub.renderVertices[0].uv);
            glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), &sub.renderVertices[0].color); // <-- Safely pointing to the array

            CCTexture2D* activeTex = nullptr;

            if (m_customTextures.count(sub.nodeName)) {
                activeTex = m_customTextures[sub.nodeName];
            } else if (sub.embeddedImageIndex >= 0 && m_embeddedTextures.count(sub.embeddedImageIndex)) {
                activeTex = m_embeddedTextures[sub.embeddedImageIndex];
            }

            if (activeTex) {
                // 1. Explicitly activate the first texture unit
                ccGLBindTexture2D(activeTex->getName());
                
                // 2. Set Wrapping to REPEAT. 
                // If your Blender UVs go outside the 0-1 box, 'Clamp' makes it a white/stretched mess.
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                
                // 3. Set Filters
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            } else {
                ccGLBindTexture2D(CCTextureCache::sharedTextureCache()->addImage("square.png", false)->getName());
            }

            glDrawElements(GL_TRIANGLES, sub.indices.size(), GL_UNSIGNED_SHORT, sub.indices.data());
        }

        if (wasCullEnabled) glEnable(GL_CULL_FACE);
    }

private:
    void loadEmbeddedTextures() {
        for (size_t i = 0; i < m_model.images.size(); ++i) {
            auto& img = m_model.images[i];
            if (!img.image.empty()) {
                CCTexture2D* tex = new CCTexture2D();
                CCTexture2DPixelFormat fmt = (img.component == 3) ? kCCTexture2DPixelFormat_RGB888 : kCCTexture2DPixelFormat_RGBA8888;
                tex->initWithData(img.image.data(), fmt, img.width, img.height, CCSize(img.width, img.height));
                m_embeddedTextures[i] = tex; 
            }
        }
    }

void extractMeshes() {
        m_subMeshes.clear();
        
        // Loop through NODES instead of meshes to support Rigid Objects
        for (size_t nIdx = 0; nIdx < m_model.nodes.size(); ++nIdx) {
            auto& gltfNode = m_model.nodes[nIdx];
            
            // If this node actually contains a mesh, extract it!
            if (gltfNode.mesh >= 0 && gltfNode.mesh < m_model.meshes.size()) {
                auto& gltfMesh = m_model.meshes[gltfNode.mesh];
                
                for (size_t pIdx = 0; pIdx < gltfMesh.primitives.size(); ++pIdx) {
                    auto& primitive = gltfMesh.primitives[pIdx];
                    SubMesh sub;
                    
                    sub.nodeIndex = nIdx; // <-- WE NOW TRACK THE PARENT NODE
                    sub.nodeName = gltfNode.name;
                    sub.name = gltfMesh.name;
                    if (sub.name.empty()) sub.name = fmt::format("mesh_{}", gltfNode.mesh);
                    if (gltfMesh.primitives.size() > 1) sub.name += fmt::format("_part_{}", pIdx);

                    if (primitive.material >= 0) {
                        auto& mat = m_model.materials[primitive.material];
                        int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
                        if (texIdx >= 0 && texIdx < m_model.textures.size()) sub.embeddedImageIndex = m_model.textures[texIdx].source;
                    }

                    extractFloatData(primitive.attributes["POSITION"], 3, false, [&](const std::vector<float>& vals) {
                        sub.basePositions.push_back({vals[0], vals[1], vals[2]});
                    });

                    if (primitive.attributes.count("TEXCOORD_0")) {
                        extractFloatData(primitive.attributes["TEXCOORD_0"], 2, false, [&](const std::vector<float>& vals) {
                            sub.uvs.push_back({vals[0], vals[1]}); 
                        });
                    } else {
                        sub.uvs.assign(sub.basePositions.size(), {0,0});
                    }

                    if (primitive.attributes.count("WEIGHTS_0")) {
                        extractFloatData(primitive.attributes["WEIGHTS_0"], 4, true, [&](const std::vector<float>& vals) {
                            sub.weights.push_back({vals[0], vals[1], vals[2], vals[3]});
                        });
                    }
                    
                    if (primitive.attributes.count("JOINTS_0")) {
                        extractFloatData(primitive.attributes["JOINTS_0"], 4, false, [&](const std::vector<float>& vals) {
                            sub.joints.push_back({vals[0], vals[1], vals[2], vals[3]});
                        });
                    }

                    if (primitive.indices >= 0) {
                        auto& accessor = m_model.accessors[primitive.indices];
                        auto& bufferView = m_model.bufferViews[accessor.bufferView];
                        auto& buffer = m_model.buffers[bufferView.buffer];
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
        }

        if (!m_model.skins.empty()) {
            auto& skin = m_model.skins[0];
            extractFloatData(skin.inverseBindMatrices, 16, false, [this](const std::vector<float>& vals) {
                Mat4 mat; for(int i=0; i<16; i++) mat.m[i] = vals[i];
                m_inverseBindMatrices.push_back(mat);
            });
        }
    }

    void buildBoneNodes() {
        m_boneNodes.clear();
        
        // Create a set of "Deform" nodes if a skin exists
        std::unordered_set<int> skeletonNodes;
        if (!m_model.skins.empty()) {
            for (int jointIdx : m_model.skins[0].joints) {
                skeletonNodes.insert(jointIdx);
            }
        }

        for (size_t i = 0; i < m_model.nodes.size(); ++i) {
            auto& gltfNode = m_model.nodes[i];
            
            // ONLY create a node if it's a bone, a mesh-container, or has children
            // This filters out cameras, lights, and stray empties
            bool isImportant = skeletonNodes.count(i) || !gltfNode.children.empty();
            
            if (isImportant) {
                auto bone = BoneNode::create(i, gltfNode.name.empty() ? fmt::format("bone_{}", i) : gltfNode.name);
                
                Vec3 t={0,0,0}, s={1,1,1}; Quat r={0,0,0,1};
                if (gltfNode.translation.size() == 3) t = {(float)gltfNode.translation[0], (float)gltfNode.translation[1], (float)gltfNode.translation[2]};
                if (gltfNode.rotation.size() == 4) r = {(float)gltfNode.rotation[0], (float)gltfNode.rotation[1], (float)gltfNode.rotation[2], (float)gltfNode.rotation[3]};
                if (gltfNode.scale.size() == 3) s = {(float)gltfNode.scale[0], (float)gltfNode.scale[1], (float)gltfNode.scale[2]};
                
                bone->updateFromGLTF(t, r, s);
                m_boneNodes[i] = bone;
            }
        }

        // Connect them
        for (size_t i = 0; i < m_model.nodes.size(); ++i) {
            if (m_boneNodes.count(i)) {
                for (int child : m_model.nodes[i].children) {
                    if (m_boneNodes.count(child)) {
                        m_boneNodes[i]->addChild(m_boneNodes[child]);
                    }
                }
            }
        }

        int sceneIndex = m_model.defaultScene > -1 ? m_model.defaultScene : 0;
        for (int rootIdx : m_model.scenes[sceneIndex].nodes) {
            if (m_boneNodes.count(rootIdx)) {
                this->addChild(m_boneNodes[rootIdx]);
            }
        }
    }

    void extractAnimations() {
        m_tracks.clear();
        m_maxAnimTime = 0.0f;

        if (m_model.animations.empty()) {
            log::warn("No animations found in this GLB file!");
            return;
        }

        // Find the first animation clip that actually contains keyframes
        const tinygltf::Animation* activeAnim = nullptr;
        for (auto& anim : m_model.animations) {
            if (!anim.channels.empty()) {
                activeAnim = &anim;
                break;
            }
        }
        if (!activeAnim) return;

        for (auto& channel : activeAnim->channels) {
            auto& sampler = activeAnim->samplers[channel.sampler];
            Track track; 
            track.targetNode = channel.target_node; 
            track.path = channel.target_path;
            track.interpolation = sampler.interpolation; // <-- Save interpolation type

            extractFloatData(sampler.input, 1, false, [&](const std::vector<float>& vals) {
                track.times.push_back(vals[0]);
                if (vals[0] > m_maxAnimTime) m_maxAnimTime = vals[0];
            });

            int numComponents = (track.path == "rotation") ? 4 : 3;
            extractFloatData(sampler.output, numComponents, false, [&](const std::vector<float>& vals) {
                track.values.push_back(vals);
            });

            m_tracks.push_back(track);
        }
    }

    void applyAnimations(float time) {
        for (auto& track : m_tracks) {
            if (track.times.empty() || track.values.empty()) continue;
            
            int frame = 0;
            int nextFrame = 0;
            float factor = 0.0f;

            if (time >= track.times.back()) {
                frame = track.times.size() - 1;
                nextFrame = frame;
                factor = 0.0f;
            } else {
                for (size_t i = 0; i < track.times.size() - 1; ++i) {
                    if (time >= track.times[i] && time < track.times[i+1]) { 
                        frame = i; 
                        nextFrame = i + 1;
                        float t0 = track.times[frame], t1 = track.times[nextFrame];
                        factor = (time - t0) / (t1 - t0);
                        break; 
                    }
                }
            }

            // --- BUG FIX: Safely jump over CUBICSPLINE tangents ---
            int vIdx0 = frame;
            int vIdx1 = nextFrame;
            
            if (track.interpolation == "CUBICSPLINE") {
                // Cubicspline outputs [InTangent, Value, OutTangent] for EVERY keyframe.
                // The actual value we want to interpolate is always the middle element!
                vIdx0 = frame * 3 + 1;
                vIdx1 = nextFrame * 3 + 1;
            }

            // Safety check in case the GLB file has corrupted data arrays
            if (vIdx1 >= track.values.size()) continue;

            auto& v0 = track.values[vIdx0]; 
            auto& v1 = track.values[vIdx1];
            auto bone = m_boneNodes[track.targetNode];

            if (track.path == "translation") {
                Vec3 t = { v0[0] + (v1[0] - v0[0]) * factor, v0[1] + (v1[1] - v0[1]) * factor, v0[2] + (v1[2] - v0[2]) * factor };
                bone->updateFromGLTF(t, bone->m_localRot, bone->m_localScale);
            } 
            else if (track.path == "scale") {
                Vec3 s = { v0[0] + (v1[0] - v0[0]) * factor, v0[1] + (v1[1] - v0[1]) * factor, v0[2] + (v1[2] - v0[2]) * factor };
                bone->updateFromGLTF(bone->m_localTrans, bone->m_localRot, s);
            } 
            else if (track.path == "rotation") {
                float dot = v0[0]*v1[0] + v0[1]*v1[1] + v0[2]*v1[2] + v0[3]*v1[3];
                float sign = (dot < 0.0f) ? -1.0f : 1.0f;

                Quat q = { 
                    v0[0] + (v1[0]*sign - v0[0]) * factor, 
                    v0[1] + (v1[1]*sign - v0[1]) * factor, 
                    v0[2] + (v1[2]*sign - v0[2]) * factor, 
                    v0[3] + (v1[3]*sign - v0[3]) * factor 
                };
                
                float mag = sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
                if (mag > 0.0001f) {
                    bone->updateFromGLTF(bone->m_localTrans, {q.x/mag, q.y/mag, q.z/mag, q.w/mag}, bone->m_localScale);
                }
            }
        }
    }

    void recalcGlobalMatrices(int nodeIndex, const Mat4& parentGlobal) {
        // FIX: If this node was filtered out during buildBoneNodes, stop here!
        if (m_boneNodes.find(nodeIndex) == m_boneNodes.end()) {
            // Even if the parent doesn't exist, the children might! 
            // We pass the parent's matrix down to them so the chain isn't broken.
            for (int child : m_model.nodes[nodeIndex].children) {
                recalcGlobalMatrices(child, parentGlobal);
            }
            return;
        }

        auto bone = m_boneNodes[nodeIndex];
        
        // Now this is safe to call because 'bone' is guaranteed not to be null
        Mat4 localMat = Mat4::fromTRS(bone->m_localTrans, bone->m_localRot, bone->m_localScale);
        bone->m_globalMat = Mat4::multiply(parentGlobal, localMat);

        if (!m_model.skins.empty()) {
            auto& skin = m_model.skins[0];
            for (size_t i = 0; i < skin.joints.size(); ++i) {
                if (skin.joints[i] == nodeIndex) {
                    m_globalJointMatrices[i] = Mat4::multiply(bone->m_globalMat, m_inverseBindMatrices[i]);
                    break;
                }
            }
        }

        for (int child : m_model.nodes[nodeIndex].children) {
            recalcGlobalMatrices(child, bone->m_globalMat);
        }
    }

    void applySkinning() {
        for (auto& sub : m_subMeshes) {
            
            // Get the global matrix of the node that this mesh belongs to
            Mat4 nodeGlobalMat;
            if (sub.nodeIndex >= 0 && m_boneNodes.count(sub.nodeIndex)) {
                nodeGlobalMat = m_boneNodes[sub.nodeIndex]->m_globalMat;
            }

            for (size_t i = 0; i < sub.basePositions.size(); ++i) {
                Vec3 base = sub.basePositions[i];
                Vec3 finalPos = {0,0,0};

                if (!sub.weights.empty() && !sub.joints.empty()) {
                    
                    // SKELETAL SKINNING (Soft vertices bending across multiple bones)
                    Vec4 w = sub.weights[i]; Vec4 j = sub.joints[i];
                    for (int k = 0; k < 4; ++k) {
                        float weight = (k==0)?w.x : (k==1)?w.y : (k==2)?w.z : w.w;
                        int jointIdx = (int)((k==0)?j.x : (k==1)?j.y : (k==2)?j.z : j.w);

                        if (weight > 0.0f && jointIdx >= 0 && jointIdx < m_globalJointMatrices.size()) {
                            Vec3 transformed = m_globalJointMatrices[jointIdx].multiplyVec3(base);
                            finalPos.x += transformed.x * weight; 
                            finalPos.y += transformed.y * weight; 
                            finalPos.z += transformed.z * weight;
                        }
                    }
                } else {
                    
                    // RIGID NODE TRANSFORM (Solid object parented to a bone)
                    // We simply multiply the vertex by the parent bone's matrix!
                    finalPos = nodeGlobalMat.multiplyVec3(base);
                }

                sub.renderVertices[i].pos = {
                    finalPos.x * GLB_PIXEL_SCALE, 
                    finalPos.y * GLB_PIXEL_SCALE, 
                    finalPos.z * GLB_PIXEL_SCALE
                };
                sub.renderVertices[i].uv = sub.uvs[i];
                
                // NEW: Apply the Cocos2D CCNodeRGBA tint directly to the vertex!
                sub.renderVertices[i].color = {
                    this->getColor().r / 255.0f, 
                    this->getColor().g / 255.0f, 
                    this->getColor().b / 255.0f, 
                    this->getOpacity() / 255.0f
                };
            }
        }
    }

    void extractFloatData(int accessorIndex, int expectedComponents, bool isWeight, std::function<void(const std::vector<float>&)> callback) {
        if (accessorIndex < 0) return;
        auto& accessor = m_model.accessors[accessorIndex];
        auto& bufferView = m_model.bufferViews[accessor.bufferView];
        auto& buffer = m_model.buffers[bufferView.buffer];
        
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
                    float v; memcpy(&v, ptr + c * 4, 4); vals[c] = v;
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    unsigned short v; memcpy(&v, ptr + c * 2, 2); vals[c] = v / normalizeFactor;
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    unsigned char v; memcpy(&v, ptr + c, 1); vals[c] = v / normalizeFactor;
                }
            }
            callback(vals);
        }
    }
};
