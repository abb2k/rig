#include <Geode/Geode.hpp>

#include <SkeletonPlayer.hpp>
#include <ModelLoader.hpp>

#include <Geode/modify/MenuLayer.hpp>
#include <geode.devtools/include/API.hpp>

using namespace geode::prelude;

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

                auto modelRes = ModelLoader::loadModel(std::string("testArmglb.glb"_spr));
                if (modelRes.isErr()){
                    log::error("{}", modelRes.unwrapErr());
                    return;
                }

                auto model = modelRes.unwrap();

                auto skeleton = SkeletonPlayer::create();
                skeleton->setID("active-skeleton");
                skeleton->loadFromGLTF(model.model);

                skeleton->setSpriteForMesh("tail", "GJ_moveBtn.png");
                
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