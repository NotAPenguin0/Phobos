#ifndef PHOBOS_FIXED_PROJECTION_HPP_
#define PHOBOS_FIXED_PROJECTION_HPP_

#include <glm/gtc/matrix_transform.hpp>
#undef near
#undef far

namespace ph::fixed {

struct Projection {
	// Near plane
	float near = 0.0f;
	// Far plane
	float far = 0.0f;
	// Field of view, in radians
	float fov = 0.0f;
	// Aspect ratio
	float aspect = 0.0f;

	inline glm::mat4 to_matrix() {
		glm::mat4 mat = glm::perspective(fov, aspect, near, far);
		mat[1][1] *= -1;
		return mat;
	}
};

struct CameraData {
	Projection projection;
	glm::vec3 position;
	glm::mat4 view;
	// Calculated automatically from projection member
	glm::mat4 projection_mat;
	glm::mat4 projection_view;
};


}

#endif