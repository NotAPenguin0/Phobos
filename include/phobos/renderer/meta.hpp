#ifndef PHOBOS_META_HPP_
#define PHOBOS_META_HPP_

namespace ph {

namespace meta {

constexpr size_t max_lights_per_type = 32;
constexpr size_t max_textures_bound = 64;

namespace bindings {

namespace generic {

constexpr size_t camera = 0;
constexpr size_t transforms = 1;
constexpr size_t lights = 2;
constexpr size_t textures = 3;

} // namespace generic

} // namespace bindings

} // namespace meta

} // namespace ph

#endif