#ifndef PHOBOS_LIGHT_HPP_
#define PHOBOS_LIGHT_HPP_

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ph {

struct alignas(2 * sizeof(glm::vec4)) PointLight {
    glm::vec3 position;
    float _pad0;
    glm::vec3 color;
    float intensity;
};

struct alignas(2 * sizeof(glm::vec4)) DirectionalLight {
    glm::vec3 direction;
    float _pad0;
    glm::vec3 color;
    float _pad1;
};

} // namespace ph 

#endif