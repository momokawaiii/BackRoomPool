#include "camera.h"


Camera::Camera(const CamInfos& CamInfos)
{
    opts = CamInfos;
    UpdateCameraVectors();
}


glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(opts.POSITION, opts.POSITION + front, up);
}


glm::mat4 Camera::GetProjectionMatrix(float aspect) const
{
    return glm::perspective(glm::radians(opts.FOV), aspect, opts.NEAR_PLANE, opts.FAR_PLANE);
}


void Camera::UpdateCameraVectors()
{
    // Calculate the new Front vector
    glm::vec3 f;
    f.x = cos(glm::radians(opts.YAW)) * cos(glm::radians(opts.PITCH));
    f.y = sin(glm::radians(opts.PITCH));
    f.z = sin(glm::radians(opts.YAW)) * cos(glm::radians(opts.PITCH));
    front = glm::normalize(f);

    // Also re-calculate the Right and Up vector
    right = glm::normalize(glm::cross(front, opts.WORLD_UP));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    up = glm::normalize(glm::cross(right, front));
}


void Camera::ProcessKeyboard(Camera_Movement moveType, float deltaTime) {
    float velocity = opts.SPEED * deltaTime;

    if (moveType == FORWARD)
        opts.POSITION += front * velocity;
    if (moveType == BACKWARD)
        opts.POSITION -= front * velocity;
    if (moveType == LEFT)
        opts.POSITION -= right * velocity;
    if (moveType == RIGHT)
        opts.POSITION += right * velocity;
    if (moveType == UP)
        opts.POSITION += opts.WORLD_UP * velocity;
    if (moveType == DOWN)
        opts.POSITION -= opts.WORLD_UP * velocity;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset) {
    xoffset *= opts.SENSITIVITY;
    yoffset *= opts.SENSITIVITY;
    opts.YAW += xoffset;
    opts.PITCH -= yoffset;
    // 限制 pitch，避免万向节锁
    if (opts.PITCH > 89.0f) opts.PITCH = 89.0f;
    if (opts.PITCH < -89.0f) opts.PITCH = -89.0f;
    UpdateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
    opts.FOV -= yoffset;
    if (opts.FOV < opts.MIN_FOV) opts.FOV = opts.MIN_FOV;
    if (opts.FOV > opts.MAX_FOV) opts.FOV = opts.MAX_FOV;
}

Camera Camera::MakeReflectedCamera(float waterHeight) const
{
    Camera reflected = *this; // 复制当前相机全部状态
    reflected.opts.POSITION.y = 2.0f * waterHeight - this->opts.POSITION.y;
    reflected.opts.PITCH = -this->opts.PITCH;
    reflected.UpdateCameraVectors();
    return reflected;
}