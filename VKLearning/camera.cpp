#include "camera.h"
#include "SDL.h"

#include <glm/gtx/transform.hpp>
void Camera::processInputEvent(const SDL_Event* ev)
{
	if (ev->type == SDL_KEYDOWN)
	{
		switch (ev->key.keysym.sym)
		{
		case SDLK_UP:
		case SDLK_s:
			inputAxis.x += 1.f;
			break;
		case SDLK_DOWN:
		case SDLK_z:
			inputAxis.x -= 1.f;
			break;
		case SDLK_LEFT:
		case SDLK_q:
			inputAxis.y -= 1.f;
			break;
		case SDLK_RIGHT:
		case SDLK_d:
			inputAxis.y += 1.f;
			break;
		case SDLK_a:
			inputAxis.z -= 1.f;
			break;

		case SDLK_e:
			inputAxis.z += 1.f;
			break;
		case SDLK_LSHIFT:
			bSprint = true;
			break;
		default:
			break;
		}
	}
	else if (ev->type == SDL_KEYUP)
	{
		switch (ev->key.keysym.sym)
		{
		case SDLK_UP:
		case SDLK_s:
			inputAxis.x -= 1.f;
			break;
		case SDLK_DOWN:
		case SDLK_z:
			inputAxis.x += 1.f;
			break;
		case SDLK_LEFT:
		case SDLK_q:
			inputAxis.y += 1.f;
			break;
		case SDLK_RIGHT:
		case SDLK_d:
			inputAxis.y -= 1.f;
			break;
		case SDLK_a:
			inputAxis.z += 1.f;
			break;

		case SDLK_e:
			inputAxis.z -= 1.f;
			break;
		case SDLK_LSHIFT:
			bSprint = false;
			break;
		default:
			break;
		}
	}
	else if (ev->type == SDL_MOUSEMOTION)
	{
		int mx, my;
		unsigned int mb = SDL_GetRelativeMouseState(&mx, &my);
		if (mb & SDL_BUTTON(1)) // le bouton gauche est enfonce
		{
			yaw += (static_cast<float>(mx) * 0.005f);
			pitch += (static_cast<float>(my) * 0.005f);
		}
	}

	inputAxis = clamp(inputAxis, { -1.0,-1.0,-1.0 }, { 1.0,1.0,1.0 });
}

void Camera::updateCamera(const float deltaSeconds)
{
	float cam_vel = speed + static_cast<float>(bSprint) * accel;
	glm::mat4 cam_rot = getRotationMatrix();

	glm::vec3 forward = { 0.f,0.f, cam_vel };
	glm::vec3 right = { cam_vel,0.f,0.f };
	glm::vec3 up = { 0.f, cam_vel, 0.f };

	forward = cam_rot * glm::vec4(forward, 0.f);
	right = cam_rot * glm::vec4(right, 0.f);

	velocity = inputAxis.x * forward + inputAxis.y * right + inputAxis.z * up;
	velocity *= deltaSeconds * 1000.f;
	position += velocity;
}


glm::mat4 Camera::getViewMatrix()
{
	glm::vec3 camPos = position;
	glm::mat4 cam_rot = getRotationMatrix();

	glm::mat4 view = translate(glm::mat4{ 1 }, camPos) * cam_rot;

	//we need to invert the camera matrix
	view = inverse(view);

	return view;
}

glm::mat4 Camera::getProjectionMatrix(bool bReverse /*= true*/)
{
	if (bReverse)
	{
		glm::mat4 pro = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 5000.0f, 0.1f);
		pro[1][1] *= -1;
		return pro;
	}
	else {
		glm::mat4 pro = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 5000.0f);
		pro[1][1] *= -1;
		return pro;
	}
}

glm::mat4 Camera::getRotationMatrix()
{
	glm::mat4 yaw_rot = rotate(glm::mat4{ 1 }, yaw, { 0,-1,0 });
	const glm::mat4 pitch_rot = rotate(glm::mat4{ yaw_rot }, pitch, { -1,0,0 });

	return pitch_rot;
}