#ifndef PHOBOS_MATERIAL_HPP_
#define PHOBOS_MATERIAL_HPP_

#include <phobos/assets/handle.hpp>
#include <phobos/renderer/texture.hpp>

namespace ph {

struct Material {
    Handle<Texture> texture;
};

} // namespace ph

#endif