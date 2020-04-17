#ifndef PHOBOS_MATERIAL_HPP_
#define PHOBOS_MATERIAL_HPP_

#include <phobos/renderer/texture.hpp>

namespace ph {

struct Material {
    Texture* diffuse = nullptr;
    Texture* specular = nullptr;
    Texture* normal = nullptr;
};

} // namespace ph

#endif