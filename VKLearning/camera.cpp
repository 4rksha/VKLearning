#include "camera.h"

#include <glm/gtx/transform.hpp>

void Camera::processKeyboard(const CameraMovement direction, const float deltaTime)
{
	const float velocity = m_movementSpeed * deltaTime;
	if (direction == FORWARD)
		m_position += m_front * velocity;
	if (direction == BACKWARD)
		m_position -= m_front * velocity;
	if (direction == LEFT)
		m_position -= m_right * velocity;
	if (direction == RIGHT)
		m_position += m_right * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset, const bool constrainPitch)
{
	xoffset *= m_sensitivity;
	yoffset *= m_sensitivity;

	m_yaw += xoffset;
	m_pitch -= yoffset;

	if (constrainPitch)
	{
		if (m_pitch > 89.0f)
			m_pitch = 89.0f;
		if (m_pitch < -89.0f)
			m_pitch = -89.0f;
	}
	updateCameraVectors();
}

void Camera::processMouseScroll(const float yoffset)
{
	m_zoom -= yoffset;
	if (m_zoom < 1.0f)
		m_zoom = 1.0f;
	if (m_zoom > 45.0f)
		m_zoom = 45.0f;
}

glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix(const float aspectRatio) const
{
	glm::mat4 pro = glm::perspective(glm::radians(m_zoom), aspectRatio, 0.1f, 100.f);
	pro[1][1] *= -1;
	return pro;

}

void Camera::updateCameraVectors()
{
	// calculate the new Front vector
	glm::vec3 front;
	front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	front.y = sin(glm::radians(m_pitch));
	front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	m_front = normalize(front);
	// also re-calculate the Right and Up vector
	m_right = normalize(cross(m_front, m_worldUp)); 
	m_up = normalize(cross(m_right, m_front));
}

