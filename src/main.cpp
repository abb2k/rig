#include <Geode/Geode.hpp>

#include <BoneNode.hpp>
#include <geode.devtools/include/API.hpp>

$on_mod(Loaded) {
    devtools::waitForDevTools([] {
        BoneNode::registerDevTools();
    });
}