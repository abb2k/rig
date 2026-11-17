#include <ModelUtils.hpp>

std::string ModelUtils::toString(tg3_str value) {
    return value.data ? std::string(value.data, value.len) : std::string();
}