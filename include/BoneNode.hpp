#pragma once

#include <Geode/Geode.hpp>
#include <Types.hpp>

using namespace geode::prelude;

class BoneNode : public CCNode {
private:
    int m_gltfIndex;
    
    Vec3 m_localTrans = Vec3::zero();
    Quat m_localRot = Quat::identity();
    Vec3 m_localScale = Vec3::one();
    Mat4 m_globalMat;
    CCPoint m_lastPos;
    float m_lastRot;

    Vec3 m_uiTrans = Vec3::zero();
    Quat m_uiRot = {0,0,0,100};
    Vec3 m_uiScale = {100, 100, 100};

    Vec3 m_devLastTrans = Vec3::zero();
    Quat m_devLastRot = {0,0,0,100};
    Vec3 m_devLastScale = {100, 100, 100};

    bool init(int gltfIndex, const std::string& name);

    void syncProxies();

    void saveState();

public:
    static BoneNode* create(int gltfIndex, const std::string& name);

    void syncFromCCNode();
    void updateFromGLTF(const std::optional<Vec3>& position, const std::optional<Quat>& rotation, const std::optional<Vec3>& scale);

    Vec3 getLocalTranslation() const { return m_localTrans; }
    void setLocalTranslation(const Vec3& pos);

    Quat getLocalRotation() const { return m_localRot; }
    void setLocalRotation(const Quat& rot);

    Vec3 getLocalScale() const { return m_localScale; }
    void setLocalScale(const Vec3& scale);

    Mat4 getGlobalMatrix() const { return m_globalMat; }
    void setGlobalMatrix(const Mat4& mat) { m_globalMat = mat; }

    static void registerDevTools();
};