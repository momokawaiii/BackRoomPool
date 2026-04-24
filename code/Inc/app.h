#pragma once
#include "renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include<memory>

class App
{
public:
  App(int width, int height, const char* title);
  ~App();
  void run();

private:
  void processInput(GLFWwindow* window);
  void render();

  static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
  static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

  float lastFrame = 0.0f;
  float deltaTime = 0.0f;
  float lastMouseX;
  float lastMouseY;
  bool firstMouse = true;

  GLFWwindow* window;
  unsigned int SCR_WIDTH;
  unsigned int SCR_HEIGHT;

  std::unique_ptr<Renderer> renderer; //RAII for Renderer
};