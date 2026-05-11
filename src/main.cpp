#include <Geode/Geode.hpp>

#include <unordered_set>
#include <cmath>
#include <vector>
#include <map>
#include <string>
#include <Geode/modify/MenuLayer.hpp>

// DevTools Headers


using namespace geode::prelude;

// Change this to scale your model up or down. 
// 100.0f means 1 Blender Meter = 100 Geometry Dash Pixels.


// ==========================================
// 1. MINI MATH ENGINE
// ==========================================


// ==========================================
// 2. PHYSICAL BONE NODE
// ==========================================


// ==========================================
// 3. SKELETON PLAYER CLASS
// ==========================================

// ==========================================
// 4. YOUR MENU LAYER MODIFICATION
// ==========================================
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto menu = this->getChildByID("bottom-menu");
        if (!menu) return true;

        auto loadMeshBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("1. Load GLB"),
            this,
            menu_selector(MyMenuLayer::onLoadGLB)
        );
        menu->addChild(loadMeshBtn);

        auto pauseBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("3. Pause/Play"),
            this,
            menu_selector(MyMenuLayer::onTogglePause)
        );
        menu->addChild(pauseBtn);

        menu->updateLayout();
        return true;
    }

    void onLoadGLB(CCObject*) {
        async::spawn(
            file::pick(
                file::PickMode::OpenFile,
                file::FilePickOptions{ .filters = { { .description = "GLB Files", .files = { "*.glb" } } } }
            ),
            [this](file::PickResult pickOpt) {
                if (pickOpt.isErr() || !pickOpt.unwrap().has_value()) return;

                auto pick = pickOpt.unwrap().value();
                tinygltf::TinyGLTF loader;
                tinygltf::Model model;
                std::string err, warn;

                bool success = loader.LoadBinaryFromFile(&model, &err, &warn, utils::string::pathToString(pick));

                if (!success) {
                    log::error("Failed to load GLB: {}", err);
                    return;
                }

                if (auto old = this->getChildByID("active-skeleton")) {
                    old->removeFromParent();
                }

                auto skeleton = SkeletonPlayer::create();
                skeleton->setID("active-skeleton");
                skeleton->loadFromGLTF(model);
                
                auto winSize = CCDirector::sharedDirector()->getWinSize();
                skeleton->setPosition(CCPoint(winSize.width / 2, winSize.height / 2 - 50.f)); 
                
                this->addChild(skeleton); 
            }
        );
    }

    void onTogglePause(CCObject*) {
        auto skeleton = static_cast<SkeletonPlayer*>(this->getChildByID("active-skeleton"));
        if (skeleton) skeleton->togglePause();
    }
};

$on_mod(Loaded) {
    devtools::waitForDevTools([] {
        BoneNode::registerDevTools();
    });
}