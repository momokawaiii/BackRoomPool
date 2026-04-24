#include "app.h"
#include <iostream>

App::App(int width, int height, const char* title) : SCR_WIDTH(width), SCR_HEIGHT(height)
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(-1);
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(
        window, [](GLFWwindow* window, int w, int h) { glViewport(0, 0, w, h); });

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        exit(-1);
    }

    // Enable depth testing for proper 3D rendering
    glEnable(GL_DEPTH_TEST);

    //renderer initialization
    renderer = std::make_unique<Renderer>(width, height);
    renderer->Init();


    // set callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);  // FPS 模式锁住鼠标
    // print versions
    // --------------
    std::cout << "--------------------------" << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "GLFW Version: " << glfwGetVersionString() << std::endl;
    std::cout << "--------------------------" << std::endl;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void App::processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    // renderer.camera control can be added here

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        renderer->GetCamera().ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        renderer->GetCamera().ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        renderer->GetCamera().ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        renderer->GetCamera().ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        renderer->GetCamera().ProcessKeyboard(DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        renderer->GetCamera().ProcessKeyboard(UP, deltaTime);
}

App::~App()
{
    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
}

//render loop
//-----------
void App::run()
{
    while (!glfwWindowShouldClose(window))
    {
        // delta time calculation
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // update
        // ------
        renderer->Update(deltaTime);

        // render
        // ------
        render();

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // --------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

}

// render function
//----------------
void App::render()
{
    glClearColor(0.1f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    //Renderer here
    renderer->Render();
}


void App::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    Camera& camera = app->renderer->GetCamera();

    if (app->firstMouse) {
        app->lastMouseX = xpos;
        app->lastMouseY = ypos;
        app->firstMouse = false;
    }

    float xoffset = xpos - app->lastMouseX;
    float yoffset = ypos - app->lastMouseY;

    app->lastMouseX = xpos;
    app->lastMouseY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void App::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    Camera& camera = app->renderer->GetCamera();
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}