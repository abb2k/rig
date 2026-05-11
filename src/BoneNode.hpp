#pragma once

#include <Geode/Geode.hpp>
#include "Types.hpp"

using namespace geode::prelude;

class BoneNode : public CCNode {
public:
    int m_gltfIndex;
    
    // The actual microscopic 3D Math Variables
    Vec3 m_localTrans = {0,0,0};
    Quat m_localRot = {0,0,0,1};
    Vec3 m_localScale = {1,1,1};
    Mat4 m_globalMat;
    float m_lastX, m_lastY, m_lastRot;

    // --- NEW: The UI "Proxy" Variables (100x larger for smooth dragging) ---
    Vec3 m_uiTrans = {0,0,0};
    Quat m_uiRot = {0,0,0,100};
    Vec3 m_uiScale = {100, 100, 100};

    // Trackers to detect DevTools edits
    Vec3 m_devLastTrans = {0,0,0};
    Quat m_devLastRot = {0,0,0,100};
    Vec3 m_devLastScale = {100, 100, 100};

    static BoneNode* create(int gltfIndex, const std::string& name) {
        auto ret = new BoneNode();
        if (ret && ret->init()) {
            ret->m_gltfIndex = gltfIndex;
            ret->setID(name);
            
            // auto label = CCLabelBMFont::create(name.c_str(), "chatFont.fnt");
            // label->setScale(0.5f);
            // label->setPosition(0, 15.f);
            // label->setOpacity(200);
            // label->setColor({ 214, 41, 41 });
            // ret->addChild(label);
            
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        // auto debugSprite = CCSprite::create("square.png");
        // if (debugSprite) {
        //     debugSprite->setOpacity(150);
        //     debugSprite->setColor({255, 0, 0});
        //     debugSprite->setScale(0.25f);
        //     this->addChild(debugSprite);
        // }
        return true;
    }

    // Sync the invisible 100x UI variables to match the actual math engine
    void syncProxies() {
        m_uiTrans = {m_localTrans.x * 100.f, m_localTrans.y * 100.f, m_localTrans.z * 100.f};
        m_uiScale = {m_localScale.x * 100.f, m_localScale.y * 100.f, m_localScale.z * 100.f};
        m_uiRot = {m_localRot.x * 100.f, m_localRot.y * 100.f, m_localRot.z * 100.f, m_localRot.w * 100.f};

        m_devLastTrans = m_uiTrans;
        m_devLastScale = m_uiScale;
        m_devLastRot = m_uiRot;
    }

    void updateFromGLTF(const Vec3& t, const Quat& r, const Vec3& s) {
        m_localTrans = t; m_localRot = r; m_localScale = s;
        
        setPosition(t.x * GLB_PIXEL_SCALE, t.y * GLB_PIXEL_SCALE);
        setScaleX(s.x); setScaleY(s.y);
        
        float eulerZ = atan2(2.0f * (r.w * r.z + r.x * r.y), 1.0f - 2.0f * (r.y * r.y + r.z * r.z));
        setRotation(-CC_RADIANS_TO_DEGREES(eulerZ));
        
        saveState();
        syncProxies();
    }

    void saveState() {
        m_lastX = getPositionX(); m_lastY = getPositionY(); m_lastRot = getRotation();
    }

    void syncFromCCNode() {
        if (getPositionX() != m_lastX || getPositionY() != m_lastY) {
            m_localTrans.x = getPositionX() / GLB_PIXEL_SCALE; 
            m_localTrans.y = getPositionY() / GLB_PIXEL_SCALE;
        }
        if (getRotation() != m_lastRot) {
            float rad = CC_DEGREES_TO_RADIANS(-getRotation());
            m_localRot.z = sin(rad / 2.0f); m_localRot.w = cos(rad / 2.0f);
            m_localRot.x = 0; m_localRot.y = 0;
        }
        
        saveState();
        syncProxies();
    }

    static void registerDevTools() {
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

            // Check if any of the UI Proxies were dragged this frame
            bool posChanged = node->m_uiTrans.x != node->m_devLastTrans.x || node->m_uiTrans.y != node->m_devLastTrans.y || node->m_uiTrans.z != node->m_devLastTrans.z;
            bool scaleChanged = node->m_uiScale.x != node->m_devLastScale.x || node->m_uiScale.y != node->m_devLastScale.y || node->m_uiScale.z != node->m_devLastScale.z;
            bool rotChanged = node->m_uiRot.x != node->m_devLastRot.x || node->m_uiRot.y != node->m_devLastRot.y || node->m_uiRot.z != node->m_devLastRot.z || node->m_uiRot.w != node->m_devLastRot.w;

            if (posChanged || scaleChanged || rotChanged) {
                
                // Gear Reduction: Divide the UI values by 100 and apply to the real Math Engine!
                node->m_localTrans = {node->m_uiTrans.x / 100.f, node->m_uiTrans.y / 100.f, node->m_uiTrans.z / 100.f};
                node->m_localScale = {node->m_uiScale.x / 100.f, node->m_uiScale.y / 100.f, node->m_uiScale.z / 100.f};
                node->m_localRot = {node->m_uiRot.x / 100.f, node->m_uiRot.y / 100.f, node->m_uiRot.z / 100.f, node->m_uiRot.w / 100.f};

                if (rotChanged) {
                    float mag = sqrt(node->m_localRot.x*node->m_localRot.x + node->m_localRot.y*node->m_localRot.y + 
                                     node->m_localRot.z*node->m_localRot.z + node->m_localRot.w*node->m_localRot.w);
                    if (mag > 0.0f) {
                        node->m_localRot.x /= mag;
                        node->m_localRot.y /= mag;
                        node->m_localRot.z /= mag;
                        node->m_localRot.w /= mag;
                    }
                }

                // Push updates to Cocos2D Screen Coordinates
                node->setPosition(node->m_localTrans.x * GLB_PIXEL_SCALE, node->m_localTrans.y * GLB_PIXEL_SCALE);
                node->setScaleX(node->m_localScale.x); 
                node->setScaleY(node->m_localScale.y);

                float eulerZ = atan2(2.0f * (node->m_localRot.w * node->m_localRot.z + node->m_localRot.x * node->m_localRot.y), 
                                     1.0f - 2.0f * (node->m_localRot.y * node->m_localRot.y + node->m_localRot.z * node->m_localRot.z));
                node->setRotation(-CC_RADIANS_TO_DEGREES(eulerZ));
                
                node->saveState();
                
                // Re-sync proxies so the sliders stay in bounds and the Quaternion normalization is reflected
                node->syncProxies(); 
            }
        });
    }
};