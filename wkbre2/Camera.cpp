#include "Camera.h"
#include "window.h"

void Camera::updateMatrix() {
	Matrix pers, cammat;
	direction = Vector3(
		-sin(orientation.y)*cos(orientation.x),
		sin(orientation.x),
		cos(orientation.y)*cos(orientation.x)
	);
	CreatePerspectiveMatrix(&pers, 0.9, (float)g_windowWidth / (float)g_windowHeight, 1.0f, 250.0f);
	CreateLookAtLHViewMatrix(&cammat, &position, &(position + direction), &Vector3(0, 1, 0));
	MultiplyMatrices(&this->sceneMatrix, &cammat, &pers);
}