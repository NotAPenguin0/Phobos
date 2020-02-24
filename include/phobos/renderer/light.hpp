#ifndef PHOBOS_LIGHT_HPP_
#define PHOBOS_LIGHT_HPP_

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ph {

struct alignas(4 * sizeof(glm::vec4)) PointLight {
    glm::vec3 position;
    float _pad0;
    glm::vec3 ambient;
    float _pad1;
    glm::vec3 diffuse;
    float _pad2;
    glm::vec3 specular;
    float _pad3;
};

} // namespace ph 

#endif