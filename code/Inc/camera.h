#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Defines several possible options for camera movement.
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
struct CamInfos {
    float YAW = -90.0f;
    float PITCH = 0.0f;
    float SPEED = 4.0f;
    float SENSITIVITY = 0.1f;
    float FOV = 45.0f;
    float MIN_FOV = 40.0f;
    float MAX_FOV = 70.0f;
    float NEAR_PLANE = 0.1f;
    float FAR_PLANE = 100.0f;
    glm::vec3 POSITION = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
};

class Camera {
    // https://learnopengl.com/Getting-started/Camera
public:
    Camera(const CamInfos& CamInfos);
    // get view and projection matrices
    // --------------------------------

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect) const;
    glm::vec4 GetPosition() const { return glm::vec4(opts.POSITION, 1.0f); }
    glm::vec2 GetNearFarPlanes() const { return glm::vec2(opts.NEAR_PLANE, opts.FAR_PLANE); }

    // process input
    // -------------
    void ProcessKeyboard(Camera_Movement moveType, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset);
    void ProcessMouseScroll(float yoffset);

    // create a reflected camera across water plane at given height
    Camera MakeReflectedCamera(float waterHeight) const;


private:
    // Camera/View space
    CamInfos opts;
    glm::vec3 front;
    glm::vec3 right;
    glm::vec3 up;

    // 根据 Euler 角更新摄像机的方向向量
    // --------------------------------
    void UpdateCameraVectors();
};


