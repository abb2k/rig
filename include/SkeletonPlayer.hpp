#pragma once

#include <Geode/Geode.hpp>

#include "Types.hpp"
#include "BoneNode.hpp"

using namespace geode::prelude;

namespace tinygltf { class Model; }

class RIG_DLL SkeletonPlayer : public CCNode, public CCRGBAProtocol {
private:
    std::shared_ptr<tinygltf::Model> m_model;
    bool m_loaded = false;
    bool m_paused = false;
    float m_time = 0.f;
    float m_maxAnimTime = 0.f;

    std::map<int, BoneNode*> m_boneNodes;

    std::vector<SubMesh> m_subMeshes;

    std::vector<Mat4> m_inverseBindMatrices;
    std::vector<Mat4> m_globalJointMatrices;

    std::map<int, CCTexture2D*> m_embeddedTextures;
    std::map<std::string, CCTexture2D*> m_customTextures;
    std::map<std::string, CCSpriteFrame*> m_customFrames;

    std::vector<Track> m_tracks;

    ccColor3B m_color = {255, 255, 255};
    GLubyte m_opacity = 255;
    bool m_opacityModifyRGB;
    bool m_cascadeColorEnabled;
    bool m_cascadeOpacityEnabled;

    void loadEmbeddedTextures();

    void extractMeshes();

    void buildBoneNodes();

    void extractAnimations();

    void applyAnimations(float time);

    void recalcGlobalMatrices(int nodeIndex, const Mat4& parentGlobal);

    void applySkinning();

    void extractFloatData(int accessorIndex, int expectedComponents, bool isWeight, std::function<void(const std::vector<float>&)> callback);

public:
    static SkeletonPlayer* create();

    bool init() override;

    ~SkeletonPlayer();

    virtual void setColor(const ccColor3B& color) override;

    virtual const ccColor3B& getColor() override;

    virtual const ccColor3B& getDisplayedColor() override;

    virtual GLubyte getDisplayedOpacity() override;

    virtual GLubyte getOpacity(void) override;

    virtual void setOpacity(GLubyte opacity) override;

    virtual void setOpacityModifyRGB(bool bValue) override;

    virtual bool isOpacityModifyRGB() override;

    virtual bool isCascadeColorEnabled() override;
    virtual void setCascadeColorEnabled(bool cascadeColorEnabled) override;

    virtual void updateDisplayedColor(const ccColor3B& color) override;

    virtual bool isCascadeOpacityEnabled() override;
    virtual void setCascadeOpacityEnabled(bool cascadeOpacityEnabled) override;
 
    virtual void updateDisplayedOpacity(GLubyte opacity) override;

    std::vector<BoneNode*> getBoneNodes();

    void togglePause();

    void setTextureForMesh(const std::string& meshName, CCTexture2D* tex);
    void setSpriteForMesh(const std::string& meshName, const std::string& spriteName);
    void setSpriteForMeshWithFrameName(const std::string& meshName, const std::string& frameName);

    void loadFromGLTF(const std::shared_ptr<tinygltf::Model>& model);

    void update(float dt) override;

    void draw() override;
};
