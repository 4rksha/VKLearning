// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"

#include <SDL_events.h>
#include <glm/glm.hpp>

enum CameraMovement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

// Default camera values
constexpr float YAW = -90.0f;
constexpr float PITCH = 0.0f;
constexpr float SPEED = 2.5f;
constexpr float SENSITIVITY = 0.1f;
constexpr float ZOOM = 45.0f;

class Camera
{
public:
	void processKeyboard(CameraMovement direction, float deltaTime);
	void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
	void processMouseScroll(float yoffset);

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix(const float aspectRatio) const;

	void setPosition(const glm::vec3 _position) { m_position = _position; }
	glm::vec3 getPosition() const { return m_position; }
	glm::vec3 getFront() const { return m_front; }

private:
	void updateCameraVectors();

	glm::vec3 m_position{ glm::vec3(0.0f, 0.0f, 3.0f) };
	glm::vec3 m_front{ glm::vec3(0.0f, 0.0f, -1.0f) };
	glm::vec3 m_up{ glm::vec3(0.0f, 1.0f, 0.0f) };
	glm::vec3 m_right{ glm::vec3(1.0f, 0.0f, 0.0f) };
	glm::vec3 m_worldUp{ glm::vec3(0.0f, 1.0f, 0.0f) };

	// euler Angles
	float m_yaw = YAW;
	float m_pitch = PITCH;
	// camera options
	float m_movementSpeed = SPEED;
	float m_sensitivity = SENSITIVITY;
	float m_zoom = ZOOM;

};