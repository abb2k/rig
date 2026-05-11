#include <BoneNode.hpp>

#include <geode.devtools/include/API.hpp>

BoneNode* BoneNode::create(int gltfIndex, const std::string& name) {
    auto ret = new BoneNode();
    if (ret && ret->init(gltfIndex, name)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool BoneNode::init(int gltfIndex, const std::string& name) {
    if (!CCNode::init()) return false;

    this->m_gltfIndex = gltfIndex;
    this->setID(name);
    
    return true;
}

void BoneNode::syncProxies() {
    m_uiTrans = m_localTrans * GLB_PIXEL_SCALE;
    m_uiScale = m_localScale * GLB_PIXEL_SCALE;
    m_uiRot = m_localRot * GLB_PIXEL_SCALE;

    m_devLastTrans = m_uiTrans;
    m_devLastScale = m_uiScale;
    m_devLastRot = m_uiRot;
}

void BoneNode::updateFromGLTF(const std::optional<Vec3>& position, const std::optional<Quat>& rotation, const std::optional<Vec3>& scale) {
    m_localTrans = position.value_or(m_localTrans);
    m_localRot = rotation.value_or(m_localRot);
    m_localScale = scale.value_or(m_localScale);
    
    this->setPosition(m_localTrans.x * GLB_PIXEL_SCALE, m_localTrans.y * GLB_PIXEL_SCALE);
    this->setScaleX(m_localScale.x);
    this->setScaleY(m_localScale.y);
    
    this->setRotation(
        -CC_RADIANS_TO_DEGREES(
            m_localRot.toEuler().z
        )
    );
    
    saveState();
    syncProxies();
}

void BoneNode::setLocalTranslation(const Vec3& pos) {
    m_localTrans = pos;
    this->setPosition(m_localTrans.x * GLB_PIXEL_SCALE, m_localTrans.y * GLB_PIXEL_SCALE);
    saveState();
    syncProxies();
}

void BoneNode::setLocalRotation(const Quat& rot) {
    m_localRot = rot;
    this->setRotation(
        -CC_RADIANS_TO_DEGREES(
            m_localRot.toEuler().z
        )
    );
    saveState();
    syncProxies();
}

void BoneNode::setLocalScale(const Vec3& scale) {
    m_localScale = scale;
    this->setScaleX(m_localScale.x);
    this->setScaleY(m_localScale.y);
    saveState();
    syncProxies();
}

void BoneNode::saveState() {
    m_lastPos = this->getPosition();
    m_lastRot = this->getRotation();
}

void BoneNode::syncFromCCNode() {
    auto currPos = this->getPosition();
    auto currRot = this->getRotation();

    if (currPos.x != m_lastPos.x || currPos.y != m_lastPos.y) {
        m_localTrans.x = currPos.x / GLB_PIXEL_SCALE; 
        m_localTrans.y = currPos.y / GLB_PIXEL_SCALE;
    }
    if (currRot != m_lastRot) {
        float rad = CC_DEGREES_TO_RADIANS(-currRot);
        m_localRot.z = sin(rad / 2.f);
        m_localRot.w = cos(rad / 2.f);
        m_localRot.x = 0;
        m_localRot.y = 0;
    }
    
    saveState();
    syncProxies();
}

void BoneNode::registerDevTools() {
    devtools::registerNode<BoneNode>([](BoneNode* node) {
        devtools::label("3D Local Position");
        devtools::property("Pos X", node->m_uiTrans.x);
        devtools::property("Pos Y", node->m_uiTrans.y);
        devtools::property("Pos Z", node->m_uiTrans.z);

        devtools::label("3D Local Scale (%)");
        devtools::property("Scale X", node->m_uiScale.x);
        devtools::property("Scale Y", node->m_uiScale.y);
        devtools::property("Scale Z", node->m_uiScale.z);

        devtools::label("3D Local Rotation (Smooth)");
        devtools::property("Rot X", node->m_uiRot.x);
        devtools::property("Rot Y", node->m_uiRot.y);
        devtools::property("Rot Z", node->m_uiRot.z);
        devtools::property("Rot W", node->m_uiRot.w);

        bool posChanged = node->m_uiTrans.x != node->m_devLastTrans.x ||
            node->m_uiTrans.y != node->m_devLastTrans.y ||
            node->m_uiTrans.z != node->m_devLastTrans.z;

        bool scaleChanged = node->m_uiScale.x != node->m_devLastScale.x ||
            node->m_uiScale.y != node->m_devLastScale.y ||
            node->m_uiScale.z != node->m_devLastScale.z;

        bool rotChanged = node->m_uiRot.x != node->m_devLastRot.x ||
            node->m_uiRot.y != node->m_devLastRot.y ||
            node->m_uiRot.z != node->m_devLastRot.z ||
            node->m_uiRot.w != node->m_devLastRot.w;

        

        if (posChanged || scaleChanged || rotChanged) {
            
            node->m_localTrans = node->m_uiTrans / GLB_PIXEL_SCALE;
            node->m_localScale =  node->m_uiScale / GLB_PIXEL_SCALE;
            node->m_localRot = node->m_uiRot / GLB_PIXEL_SCALE;

            if (rotChanged) {
                float mag = node->m_localRot.magnetude();
                
                if (mag > 0.f)
                    node->m_localRot /= mag;
            }

            node->setPosition(node->m_localTrans.x * GLB_PIXEL_SCALE, node->m_localTrans.y * GLB_PIXEL_SCALE);
            node->setScaleX(node->m_localScale.x);
            node->setScaleY(node->m_localScale.y);

            node->setRotation(
                -CC_RADIANS_TO_DEGREES(
                    node->m_localRot.toEuler().z
                )
            );
            
            node->saveState();
            
            node->syncProxies(); 
        }
    });
}