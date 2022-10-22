#include "Include/Renderer.h"

// std::function<void()> loop;
// void main_loop() { loop(); }

Renderer::Renderer()
{

}

/**
 * Start Run
 */
void Renderer::Run()
{
    InitializeSDL();
    InitializeImGui();
    InitializeVariables();
    CreateOpenGLProgram("Shaders/shader.vert", "Shaders/shader.frag");
    InitializeGeometry();
    MainLoop();
    Cleanup();
}
/**
 * End Run
 */

/**
 * Start SDL
 */
void Renderer::InitializeSDL()
{
    SDL_CreateWindowAndRenderer(Renderer::WIDTH, Renderer::HEIGHT, 0, &(Renderer::window), nullptr);
    
    Renderer::context = SDL_GL_CreateContext(Renderer::window);
    SDL_GL_MakeCurrent(Renderer::window, Renderer::context);
    if (!Renderer::context)
    {
        throw std::runtime_error("Failed to create GL context!");
    } 
    SDL_GL_SetSwapInterval(1); // Enable vsync

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
}
/**
 * End SDL
 */

/**
 * Start ImGui
 */
void Renderer::InitializeImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = NULL;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(Renderer::window, Renderer::context);
    ImGui_ImplOpenGL3_Init(Renderer::glsl_version);
}
/**
 * End ImGui
 */

/**
 * Start Initialize Variables
 */
void Renderer::InitializeVariables()
{
    Renderer::clicked = false;
    Renderer::SPEED = 1.0f;
    Renderer::camPos = glm::vec3( 0.0f, 0.0f, 3.0f ); 
    Renderer::camFront = glm::vec3( 0.0f, 0.0f, -1.0f );
    Renderer::camUp = glm::vec3( 0.0f, 1.0f, 0.0f );
    Renderer::time = 0.0f;
    Renderer::firstMouse = true;
    Renderer::lastX = (float)Renderer::WIDTH / 2.0f;
    Renderer::lastY = (float)Renderer::HEIGHT;
    Renderer::yaw = -90.f;
    Renderer::pitch = 0.0f;
    Renderer::deltaTime = 0.0f;
    Renderer::lastFrame = 0.0f;
}
/**
 * End Initialize Variables
 */

/**
 * Start Create OpenGL Program
 */
void Renderer::CreateOpenGLProgram(const char* vertexPath, const char* fragmentPath)
{
    Shader createShader = Shader(vertexPath, fragmentPath);
    Renderer::shader = &createShader;
    Renderer::shader->use();
}

void Renderer::InitializeGeometry()
{
    float vertices[] = {
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f, // top right
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f   // top left
    };
    unsigned int indices[] = {  // note that we start from 0!
        0, 1, 3,  // first Triangle
        1, 2, 3   // second Triangle
    };

    // Create a Vertex Buffer Object and copy the vertex data to it
    unsigned int VBO, EBO;
    glGenVertexArraysOES(1, &(Renderer::VAO));
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArrayOES(Renderer::VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArrayOES(0);
}
/**
 * End Create OpenGL Program
 */

/**
 * Start Process Input
 */
void Renderer::ProcessInput()
{
    float camSpeed = Renderer::deltaTime * Renderer::SPEED;
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        
        switch(event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_w:
                        Renderer::camPos += camSpeed * Renderer::camFront;
                        //std::cout << "Pressed W" <<"\n";
                        break;
                    case SDLK_s:
                        Renderer::camPos -= camSpeed * Renderer::camFront;
                        //std::cout << "Pressed S" <<"\n";
                        break;
                    case SDLK_a:
                        Renderer::camPos -= camSpeed * glm::normalize(glm::cross(Renderer::camFront, Renderer::camUp));
                        //std::cout << "Pressed A" <<"\n";
                        break;
                    case SDLK_d:
                        Renderer::camPos += camSpeed * glm::normalize(glm::cross(Renderer::camFront, Renderer::camUp));
                        //std::cout << "Pressed D" <<"\n";
                        break;
                    break;
                }
        }
    }
}
/**
 * End Process Input
 */

/**
 * Start Game Loop
 */
void Renderer::MainLoop()
{
    loop = [&]
    {
        Renderer::ProcessInput();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin( "Graphical User Interface" );   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
		ImGui::Text( "Testing");
        ImGui::SliderFloat( "Test", &(Renderer::test), 0.0f, 1.0f );
        ImGui::End();

        // Calculate the time between frames.
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        Renderer::time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

        Renderer::deltaTime = Renderer::time - Renderer::lastFrame;
        Renderer::lastFrame = Renderer::time;

        glm::vec2 widthHeight = glm::vec2(Renderer::screenWidth, Renderer::screenHeight);

        // No need to compute this every frame as the FOV stays always the same.
		glm::mat4 projection = glm::perspective( glm::radians( 45.0f ), (float) Renderer::WIDTH / (float) Renderer::HEIGHT, //widthHeight.x / widthHeight.y,
												 0.1f, 100.0f
												);
        glm::mat4 model = glm::mat4(1.0f);
        // render
        // ------

        glClearColor(1.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Create the camera (eye).
        glm::mat4 view = glm::lookAt(Renderer::camPos, Renderer::camPos + Renderer::camFront, Renderer::camUp);

        // draw our first triangle
        Renderer::shader->use();
        glBindVertexArrayOES(Renderer::VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized        
        
        //glDrawArrays(GL_TRIANGLES, 0, 6);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
 
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(Renderer::window);

        //int x, y;
        Uint32 buttons;

        SDL_PumpEvents();  // make sure we have the latest mouse state.

        Renderer::shader->setMat4("model", model);
        Renderer::shader->setMat4("projection", projection);
        Renderer::shader->setMat4("view", view);

        buttons = SDL_GetMouseState(&(Renderer::mousePositionX), &(Renderer::mousePositionY));
        glm::vec3 mouse = glm::vec3(Renderer::mousePositionX, -Renderer::mousePositionY + Renderer::screenHeight, (Renderer::clicked ? 1.0f : 0.0f));
        Renderer::shader->setVec3("iMouse", mouse);

        glm::vec2 texelSize   = glm::vec2(1.0f) / widthHeight;
        Renderer::shader->setVec4("iResolution", glm::vec4(widthHeight, texelSize));

        Renderer::shader->setFloat("iTest", Renderer::test);

        SDL_GL_GetDrawableSize(Renderer::window, &(Renderer::screenWidth), &(Renderer::screenHeight));
        if ((buttons & SDL_BUTTON_LMASK) != 0) 
        {
            Renderer::clicked = true;
            std::cout << "Mouse Button 1 (left) is pressed.\n";
        }
        else
        {
            Renderer::clicked = false;
        }

        if(Renderer::firstMouse || !Renderer::clicked)
		{
			Renderer::lastX = mouse.x;
			Renderer::lastY = mouse.y;
			Renderer::firstMouse = false;
		}

		float xoffset = mouse.x - Renderer::lastX;
		float yoffset = Renderer::lastY - mouse.y; // reversed since y-coordinates go from bottom to top
		Renderer::lastX = mouse.x;
		Renderer::lastY = mouse.y;

		float sensitivity = 0.1f; // Who doesn't like magic values?
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		Renderer::yaw += xoffset;
		Renderer::pitch += yoffset;

		// make sure that when pitch is out of bounds, screen doesn't get flipped
		if (Renderer::pitch > 89.0f)
			Renderer::pitch = 89.0f;
		if (Renderer::pitch < -89.0f)
			Renderer::pitch = -89.0f;

		// Kill me math wizards, or lock me then Gimbal...
		glm::vec3 front;
		front.x = cos(glm::radians(Renderer::yaw)) * cos(glm::radians(Renderer::pitch));
		front.y = sin(glm::radians(Renderer::pitch));
		front.z = sin(glm::radians(Renderer::yaw)) * cos(glm::radians(Renderer::pitch));
		Renderer::camFront = glm::normalize(front);
    };
    //emscripten_set_main_loop(main_loop, 0, true);
}
/**
 * End Game Loop
 */

/**
 * Start Cleanup
 */
void Renderer::Cleanup()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}
/**
 * End Cleanup
 */

