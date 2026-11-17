#pragma once

#include <Geode/Geode.hpp>
#include "tiny_gltf_v3.h"

#include "Types.hpp"

using namespace geode::prelude;

class RIG_DLL ModelUtils {
public:
    static std::string toString(tg3_str value);
};