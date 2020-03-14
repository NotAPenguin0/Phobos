#ifndef PHOBOS_MATERIAL_HPP_
#define PHOBOS_MATERIAL_HPP_

#include <phobos/renderer/texture.hpp>

namespace ph {

struct Material {
   Texture* texture;
};

} // namespace ph

#endif