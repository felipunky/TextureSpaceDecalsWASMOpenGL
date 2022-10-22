#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include <emscripten.h>
#include <iostream>
#include <functional>
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengles2.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <iostream>

#pragma once
#include "Shader.h"

class Renderer
{
private:
    const int WIDTH = 640,
                HEIGHT = 480;

    int screenWidth, screenHeight;

    SDL_Window *window;
    SDL_GLContext context;
    const char* glsl_version = "#version 100";

    int mousePositionX, 
        mousePositionY;
    bool mouseInsideOfWindow;
    bool clicked = false;

    float SPEED = 1.0f;
    glm::vec3 camPos, 
                camFront,
                camUp;

    float time;
    // Mouse rotation globals.
    bool firstMouse;
    float lastX, 
            lastY, 
            yaw, 
            pitch;

    // How much time between frames.
    float deltaTime, 
            lastFrame;

    float test;

    unsigned int VAO;
    Shader* shader;

    void InitializeSDL();
    void InitializeImGui();
    void InitializeVariables();
    void CreateOpenGLProgram(const char* vertexPath, const char* fragmentPath);
    void InitializeGeometry();
    void ProcessInput();
    void MainLoop();
    void Cleanup();

public:
    Renderer();
    void Run();
};