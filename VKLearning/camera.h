// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"

#include <SDL_events.h>
#include <glm/glm.hpp>


struct Camera {
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 inputAxis;

	float speed = 0.000002f;
	float accel = 0.00001f;

	float pitch{ 0 }; //up-down rotation
	float yaw{ 0 }; //left-right rotation

	bool bSprint = false;
	bool bLocked;

	void processInputEvent(const SDL_Event* ev);
	void updateCamera(float deltaSeconds);

	glm::mat4 getViewMatrix();
	glm::mat4 getProjectionMatrix(bool bReverse = true);
	glm::mat4 getRotationMatrix();
};