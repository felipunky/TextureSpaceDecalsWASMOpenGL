#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "ImGuizmo.h"

#include <emscripten.h>
#include <GLES3/gl3.h>
#include <functional>
#include <string>
#include <bitset>
#include <unordered_map>

#include <chrono>
#include "Include/Shader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#if 0
#define glGenVertexArrays glGenVertexArraysOES
#define glBindVertexArray glBindVertexArrayOES
#endif

uint8_t* decalImageBuffer;
uint16_t widthDecal = 0, heightDecal = 0, clicked = 0;

uint8_t* loadArray(uint8_t* buf, int bufSize)
{
    //size_t size = (sizeof(buf) / sizeof(buf[0]));
    std::cout << "Buffer size: " << bufSize << std::endl;
    uint8_t* newBuf = new uint8_t[bufSize];
    for (int i = 0; i < bufSize; ++i)
    {
        newBuf[i] = buf[i];
    }
    return newBuf;
}

extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void load (uint8_t* buf, int bufSize) 
    {
        //printf("[WASM] Loading Texture \n");
        std::cout << "Reading decal image!" << std::endl;
        std::cout << "Decal buffer size: " << bufSize << std::endl;
        
        decalImageBuffer = loadArray(buf, bufSize);
        
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passSize (uint16_t* buf, int bufSize)
    {
        std::cout << "Reading decal image size!" << std::endl;
        widthDecal = buf[0];
        heightDecal = buf[1];
        clicked = buf[2];
        std::cout << "Decal Width: " << +widthDecal << std::endl;
        std::cout << "Decal Height: " << +heightDecal << std::endl;
        std::cout << "Clicked :" << +clicked << std::endl;
        free(buf);
    }
}

bool useWindow = false;
float camDistance = 1.f;
static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);

const int WIDTH = 1200,
          HEIGHT = 600;

// an example of something we will control from the javascript side

std::function<void()> loop;
void main_loop() { loop(); }

std::vector<glm::vec3> modelDataVertices;
std::vector<glm::vec3> modelDataNormals;
std::vector<glm::vec2> modelDataTextureCoordinates;
std::vector<tinyobj::material_t> modelDataMaterial;
std::map<std::string, GLuint> modelDataTextures;
std::vector<unsigned int> indexes;
glm::vec3 bboxMin = glm::vec3(1e+5);
glm::vec3 bboxMax = glm::vec3(-1e+5);
glm::vec3 centroid;

int lastUsing = 0;

struct Material
{
    unsigned int baseColor;
    unsigned int normal;
    unsigned int roughness;
    unsigned int metallic;
    unsigned int ao;
};

Material material;

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void EditTransform(float* cameraView, float* cameraProjection, float* matrix, bool editTransformDecomposition, ImGuiIO& io)
{
   static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
   static bool useSnap = false;
   static float snap[3] = { 1.f, 1.f, 1.f };
   static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
   static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
   static bool boundSizing = false;
   static bool boundSizingSnap = false;

   if (editTransformDecomposition)
   {
      if (ImGui::IsKeyPressed(ImGuiKey_T))
         mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
      if (ImGui::IsKeyPressed(ImGuiKey_R))
         mCurrentGizmoOperation = ImGuizmo::ROTATE;
      if (ImGui::IsKeyPressed(ImGuiKey_R)) // r Key
         mCurrentGizmoOperation = ImGuizmo::SCALE;
      if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
         mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
      ImGui::SameLine();
      if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
         mCurrentGizmoOperation = ImGuizmo::ROTATE;
      ImGui::SameLine();
      if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
         mCurrentGizmoOperation = ImGuizmo::SCALE;
      if (ImGui::RadioButton("Universal", mCurrentGizmoOperation == ImGuizmo::UNIVERSAL))
         mCurrentGizmoOperation = ImGuizmo::UNIVERSAL;
      float matrixTranslation[3], matrixRotation[3], matrixScale[3];
      ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
      ImGui::InputFloat3("Tr", matrixTranslation);
      ImGui::InputFloat3("Rt", matrixRotation);
      ImGui::InputFloat3("Sc", matrixScale);
      ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

      if (mCurrentGizmoOperation != ImGuizmo::SCALE)
      {
         if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
            mCurrentGizmoMode = ImGuizmo::LOCAL;
         ImGui::SameLine();
         if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
            mCurrentGizmoMode = ImGuizmo::WORLD;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_P))
         useSnap = !useSnap;
      ImGui::Checkbox("##UseSnap", &useSnap);
      ImGui::SameLine();

      switch (mCurrentGizmoOperation)
      {
      case ImGuizmo::TRANSLATE:
         ImGui::InputFloat3("Snap", &snap[0]);
         break;
      case ImGuizmo::ROTATE:
         ImGui::InputFloat("Angle Snap", &snap[0]);
         break;
      case ImGuizmo::SCALE:
         ImGui::InputFloat("Scale Snap", &snap[0]);
         break;
      }
      ImGui::Checkbox("Bound Sizing", &boundSizing);
      if (boundSizing)
      {
         ImGui::PushID(3);
         ImGui::Checkbox("##BoundSizing", &boundSizingSnap);
         ImGui::SameLine();
         ImGui::InputFloat3("Snap", boundsSnap);
         ImGui::PopID();
      }
   }

   //ImGuiIO& io = ImGui::GetIO();
   float viewManipulateRight = io.DisplaySize.x;
   float viewManipulateTop = 0;
   static ImGuiWindowFlags gizmoWindowFlags = 0;
   if (useWindow)
   {
      ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Appearing);
      ImGui::SetNextWindowPos(ImVec2(400,20), ImGuiCond_Appearing);
      ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImVec4)ImColor(0.35f, 0.3f, 0.3f));
      ImGui::Begin("Gizmo", 0, gizmoWindowFlags);
      ImGuizmo::SetDrawlist();
      float windowWidth = (float)ImGui::GetWindowWidth();
      float windowHeight = (float)ImGui::GetWindowHeight();
      ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);
      viewManipulateRight = ImGui::GetWindowPos().x + windowWidth;
      viewManipulateTop = ImGui::GetWindowPos().y;
      ImGuiWindow* window = ImGui::GetCurrentWindow();
      gizmoWindowFlags = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(window->InnerRect.Min, window->InnerRect.Max) ? ImGuiWindowFlags_NoMove : 0;
   }
   else
   {
      ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
   }
   ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
}

// http://marcelbraghetto.github.io/a-simple-triangle/2019/04/14/part-09/
std::string LoadTextFile(const std::string& path)
{
    SDL_RWops* file{SDL_RWFromFile(path.c_str(), "r")};
    size_t fileLength{static_cast<size_t>(SDL_RWsize(file))};
    void* data{SDL_LoadFile_RW(file, nullptr, 1)};
    std::string result(static_cast<char*>(data), fileLength);
    SDL_free(data);

    return result;
}

std::bitset<256> VertexBitHash(glm::vec3* v, glm::vec3* n, glm::vec2* u) {
    std::bitset<256> bits{};
    bits |= reinterpret_cast<unsigned &>(v->x); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(v->y); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(v->z); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(n->x); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(n->y); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(n->z); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(u->x); bits <<= 32;
    bits |= reinterpret_cast<unsigned &>(u->y);
    return bits;
}

void ObjLoader(Shader* shader)
{
    //std::string inputfile = "Assets/dickies_long_sleeve_shirt/shirt.obj";
	//std::string inputfile = "Assets/crate.obj";
    //std::string inputfile = "Assets/shirtTwo.obj";
    //std::string inputfile = "Assets/Shirt/BetterTShirt/BetterShirt.obj";
    std::string inputfile = "Assets/t-shirt-lp/source/Shirt.obj";

	std::istringstream stream(LoadTextFile(inputfile));
    //membuf sbuf(objFile, nullptr);
    //std::istream stream(&sbuf);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;

    if (!tinyobj::LoadObj(
            &attrib,
            &shapes,
            &materials,
            &error,
            &warning,
            &stream,
            NULL,
            true))
    {
        throw std::runtime_error("loadOBJFile: Error: " + warning + error);
    }

    std::unordered_map<std::bitset<256>, uint32_t> uniqueVertices;

    uint32_t counter = 0;

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            glm::vec3 position{
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]};
            glm::vec3 normal{
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]};
            glm::vec2 textureCoordinates{
                attrib.texcoords[2 * abs(index.texcoord_index) + 0],
                attrib.texcoords[2 * abs(index.texcoord_index) + 1]};
            auto hash = VertexBitHash(&position, &normal, &textureCoordinates);
            if (uniqueVertices.count(hash) == 0)
            {
                modelDataVertices.push_back(position);
                modelDataNormals.push_back(normal);
                modelDataTextureCoordinates.push_back(textureCoordinates);
                // BBox
                bboxMax = glm::max(bboxMax, position);
                bboxMin = glm::min(bboxMin, position);
                indexes.push_back(counter);
                uniqueVertices[hash] = counter;
                ++counter;
            }
            else
            {
                indexes.push_back(uniqueVertices[hash]);
            }
        }
    }

    // shader->createTexture(&(material.baseColor), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_basecolor.png", "BaseColor", 0);
    // shader->createTexture(&(material.normal), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_normal.png", "Normal", 1);
    // shader->createTexture(&(material.roughness), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_roughness.png", "Roughness", 2);
    // shader->createTexture(&(material.metallic), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_metallic.png", "Metallic", 3);

    shader->createTexture(&(material.baseColor), "Assets/t-shirt-lp/textures/T_shirt_albedo.jpg", "BaseColor", 0);
    shader->createTexture(&(material.normal), "Assets/t-shirt-lp/textures/T_shirt_normal.png", "Normal", 1);
    shader->createTexture(&(material.roughness), "Assets/t-shirt-lp/textures/T_shirt_roughness.jpg", "Roughness", 2);
    shader->createTexture(&(material.metallic), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_metallic.png", "Metallic", 3);
    shader->createTexture(&(material.ao), "Assets/t-shirt-lp/textures/T_shirt_AO.jpg", "AmbientOcclussion", 4);

    std::cout << "Vertices: " << modelDataVertices.size() << "\n";
    std::cout << "Normals: " << modelDataNormals.size() << "\n";
    std::cout << "Texture Coordinates: " << modelDataTextureCoordinates.size() << "\n";
    std::cout << "Materials: " << materials.size() << "\n";
    std::cout << "BboxMax: {x: " << bboxMax.x << ", y: " << bboxMax.y << ", z: " << bboxMax.z << "}\n";
    std::cout << "BboxMin: {x: " << bboxMin.x << ", y: " << bboxMin.y << ", z: " << bboxMin.z << "}\n";
}

int main()
{
    /**
     * Start Window
     */
    int screenWidth, screenHeight;
    
    SDL_Window* window;
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, nullptr);
    
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    if (!context)
    {
        throw std::runtime_error("Failed to create GL context!");
    } 

    SDL_GL_SetSwapInterval(1); // Enable vsync

    const char* glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,16);
    /**
     * End Window
     */
    /**
     * Start ImGui
     */
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = NULL;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    /**
     * End ImGui
     */
    
    /**
     * Start User Interaction
     */
	// OpenGL is right handed so the last component corresponds to move forward/backwards.
    int mousePositionX, mousePositionY;
    bool mouseInsideOfWindow;
    bool clicked = false;

	const float SPEED = 5.0f;
	glm::vec3 camPos = glm::vec3( 0.0f, 0.0f, 1.0f ); 
	glm::vec3 camFront = glm::vec3( 0.0f, 0.0f, -1.0f );
	glm::vec3 camUp = glm::vec3( 0.0f, 1.0f, 0.0f );

    float time = 0.0f;
	// Mouse rotation globals.
	bool firstMouse = true;
	float lastX = ( float )( WIDTH ) / 2.0F, 
          lastY = ( float )( HEIGHT ), 
          yaw = -90.0f, 
          pitch = 0.0f;

	// How much time between frames.
	float deltaTime = 0.0f, lastFrame = 0.0f;
    /**
     * End User Interaction
     */

    /**
     * Start Shader Setup
     */
    Shader geometryPass = Shader("Shaders/GBuffer.vert", "Shaders/GBuffer.frag");   
    Shader deferredPass = Shader("Shaders/DeferredPass.vert", "Shaders/DeferredPass.frag");
    Shader decalsPass   = Shader("Shaders/Decals.vert", "Shaders/Decals.frag");
    Shader bakePass     = Shader("Shaders/Baker.vert", "Shaders/Baker.frag");
    decalsPass.use();
    decalsPass.setInt("iDepth", 0);
    

    //unsigned int decalNormals;
    //decalsPass.createTexture(&decalNormals, "");

    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, 
                 gNormal, 
                 gAlbedo, 
                 gMetallic, 
                 gRoughness, 
                 gAO, 
                 rboDepth;
    unsigned int baker;

     // create and attach depth buffer (renderbuffer)
    glGenTextures(1, &rboDepth);
    glBindTexture(GL_TEXTURE_2D, rboDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, WIDTH/2, HEIGHT, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, rboDepth, 0);
    
    // - position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    
    // - normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    
    // - color buffer
    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

    // - metallic buffer
    glGenTextures(1, &gMetallic);
    glBindTexture(GL_TEXTURE_2D, gMetallic);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gMetallic, 0);
    
    // - roughness buffer
    glGenTextures(1, &gRoughness);
    glBindTexture(GL_TEXTURE_2D, gRoughness);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gRoughness, 0);

    // - AO buffer
    glGenTextures(1, &gAO);
    glBindTexture(GL_TEXTURE_2D, gAO);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, gAO, 0);

    // - Baker buffer
    glGenTextures(1, &baker);
    glBindTexture(GL_TEXTURE_2D, baker);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH/2, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D, baker, 0);

    // - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
    glDrawBuffers(6, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    deferredPass.use();
    deferredPass.setInt("gPosition", 0);
    deferredPass.setInt("gNormal", 1);
    deferredPass.setInt("gAlbedo", 2);
    deferredPass.setInt("gMetallic", 3);
    deferredPass.setInt("gRoughness", 4);
    deferredPass.setInt("gAO", 5);

    bakePass.use();
    ObjLoader(&bakePass);

    /**
     * End Shader Setup
     */

    /**
     * Start Geometry Definition
     */
    geometryPass.use();
	ObjLoader(&geometryPass);

    // Create a Vertex Buffer Object and copy the vertex data to it
    unsigned int VBOVertices, VBONormals, VBOTextureCoordinates, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBOVertices);
	glBindBuffer(GL_ARRAY_BUFFER, VBOVertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * modelDataVertices.size(), modelDataVertices.data(), GL_STATIC_DRAW);
    modelDataVertices.clear();

	glGenBuffers(1, &VBONormals);
	glBindBuffer(GL_ARRAY_BUFFER, VBONormals);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * modelDataNormals.size(), modelDataNormals.data(), GL_STATIC_DRAW);
    modelDataNormals.clear();

	glGenBuffers(1, &VBOTextureCoordinates);
	glBindBuffer(GL_ARRAY_BUFFER, VBOTextureCoordinates);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * modelDataTextureCoordinates.size(), modelDataTextureCoordinates.data(), GL_STATIC_DRAW);
    modelDataTextureCoordinates.clear();

    glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indexes.size(), indexes.data(), GL_STATIC_DRAW);
    //indexes.clear();
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, VBOVertices);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, VBONormals);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, VBOTextureCoordinates);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    //glEnable(GL_MULTISAMPLE_RASTERIZATION_ALLOWED_EXT); 
    glEnable(GL_DEPTH_TEST); 
    /**
     * End Geometry Definition
     */

    bool show_demo_window = true;
    
    float test = 1.0f;
	bool insideImGui = false;
    

	// Create the camera (eye).
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 model = glm::mat4(1.0f);
    centroid = bboxMax * glm::vec3(0.5) + bboxMin * glm::vec3(0.5);
    model = glm::translate(model, -centroid);

    float iTime = 0.0f;

    unsigned int decalTexture = -1;
    //decalsPass.createTextureFromFile(&decalTexture, decalImageBuffer, widthDecal, heightDecal, "iChannel0", 1);
    //decalsPass.createTextureFromFile(&decalTexture, decalImageBuffer, widthDecal, heightDecal, "iChannel0", 1);
    decalsPass.createTexture(&decalTexture, "Assets/Textures/Batman.png", "iChannel0", 1);
    //std::cout << "Decal Texture binding: " << decalTexture << std::endl;

    int flip = 0;

    int click = 0;

    int halfWidth = WIDTH/2, halfHeight = HEIGHT/2;

    loop = [&]
    {

        if (decalImageBuffer)//decalImageBuffer && clicked == 1)
        {
            //decalsPass.createTexture(&decalTexture, "Assets/Textures/WatchMen.jpeg", "iChannel0", 1);
            //decalsPass.createTextureFromFile(&decalTexture, decalImageBuffer, widthDecal, heightDecal, "iChannel0", 1);
            flip = 1;
            glGenTextures(1, &decalTexture);
            glBindTexture(GL_TEXTURE_2D, decalTexture);

            // In an ideal world this should be exposed as input params to the function.
            // Texture wrapping params.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            // Texture filtering params.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Get the texture format automatically.
            auto format = GL_RGBA;
            if (decalImageBuffer)
            {
                std::cout << "Buffer not zero" << std::endl;
                glTexImage2D(GL_TEXTURE_2D, 0, format, widthDecal, heightDecal, 0, format, GL_UNSIGNED_BYTE, decalImageBuffer);
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            else
            {
                throw std::runtime_error("Failed to load texture!");
            }
            // Clear the data.
            free(decalImageBuffer);
            //buffer.clear();

            // Bind the uniform sampler.
            decalsPass.use();
            decalsPass.setInt("iChannel0", 1);
            // //this->setInt(samplerName, uniform);
        }
        /*else
        {
            decalsPass.createTexture(&decalTexture, "Assets/Textures/WatchMen.jpeg", "iChannel0", 1);
            //decalsPass.createTextureFromFile(&decalTexture, decalImageBuffer, widthDecal, heightDecal, "iChannel0", 1);
        }*/

        glEnable(GL_DEPTH_TEST);
        /**
         * Start ImGui
         */	        
        float camSpeed = deltaTime * SPEED;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);

			if (io.WantCaptureMouse || io.WantCaptureKeyboard) 
			{
				insideImGui = true;
				//std::cout << "Inside ImGui window!\n";
            	//break;
        	}
			else
			{
				insideImGui = false;
			}

			if (insideImGui)
			{
				break;
			}
            
            switch(event.type)
            {
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_w:
                            camPos += camSpeed * camFront;
                            //std::cout << "Pressed W" <<"\n";
                            break;
                        case SDLK_s:
                            camPos -= camSpeed * camFront;
                            //std::cout << "Pressed S" <<"\n";
                            break;
                        case SDLK_a:
                            camPos -= camSpeed * glm::normalize(glm::cross(camFront, camUp));
                            //std::cout << "Pressed A" <<"\n";
                            break;
                        case SDLK_d:
                            camPos += camSpeed * glm::normalize(glm::cross(camFront, camUp));
                            //std::cout << "Pressed D" <<"\n";
                            break;
                        break;
                    }
            }
        }

        glm::vec2 widthHeight = glm::vec2(screenWidth/2, screenHeight);
        glm::vec3 mouse;
        if (!insideImGui)
		{
            //int x, y;
            Uint32 buttons;
            buttons = SDL_GetMouseState(&mousePositionX, &mousePositionY);
            mouse = glm::vec3(mousePositionX, -mousePositionY + screenHeight, (clicked ? 1.0f : 0.0f));
			if ((buttons & SDL_BUTTON_LMASK) != 0) 
			{
				clicked = true;
				std::cout << "Mouse Button 1 (left) is pressed.\n";
			}
			else
			{
				clicked = false;
			}

			if(firstMouse || !clicked)
			{
				lastX = mouse.x;
				lastY = mouse.y;
				firstMouse = false;
			}

			float xoffset = mouse.x - lastX;
			float yoffset = lastY - mouse.y; // reversed since y-coordinates go from bottom to top
			lastX = mouse.x;
			lastY = mouse.y;

			float sensitivity = 0.1f; // Who doesn't like magic values?
			xoffset *= sensitivity;
			yoffset *= sensitivity;

			yaw += xoffset;
			pitch += yoffset;

			// make sure that when pitch is out of bounds, screen doesn't get flipped
			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;

			// Kill me math wizards, or lock me then Gimbal...
			glm::vec3 front;
			front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
			front.y = sin(glm::radians(pitch));
			front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
			camFront = glm::normalize(front);
		}

        view = glm::lookAt( camPos, camPos + camFront, camUp );

        // No need to compute this every frame as the FOV stays always the same.
		glm::mat4 projection = glm::perspective( glm::radians( 45.0f ), widthHeight.x / widthHeight.y,
												 0.1f, 200.0f
												);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        ImGui::Begin( "Graphical User Interface" );   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        auto printTime = (std::to_string(deltaTime * 1000.0f) + " ms.\n").c_str();
		ImGui::Text(printTime);
        ImGui::SliderFloat( "Test", &test, 1.0f, 100.0f );
        if (ImGui::Button("Click me")) 
        {
            if (click == 0)
            {
                click = 1;
            }
            else
            {
                click = 0;
            }
        }        

        for (int i = 0; i < 1; ++i)
        {
            ImGuizmo::SetID(i);
            EditTransform(glm::value_ptr(view), glm::value_ptr(projection), glm::value_ptr(model), lastUsing == i, io);
            if (ImGuizmo::IsUsing())
            {
                lastUsing = i;
            }
        }
        
        ImGui::End();

        // Calculate the time between frames.
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        time = std::chrono::duration<float, std::chrono::seconds::period>( currentTime - startTime ).count();

        deltaTime = time - lastFrame;
        lastFrame = time;

        // render
        // ------
        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer); 
        
        geometryPass.use();
        glm::mat4 mm = glm::mat4(1.0f);
        geometryPass.setMat4("model", mm);
        geometryPass.setMat4("projection", projection);
        geometryPass.setMat4("view", view);
        glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.baseColor);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.roughness);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material.metallic);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, material.ao);

        glEnable(GL_SCISSOR_TEST);
        glViewport(0, 0, screenWidth/2, screenHeight);
        glScissor(0, 0, screenWidth/2, screenHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        // 2. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content.
        // -----------------------------------------------------------------------------------------------------------------------
        
        deferredPass.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedo);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, gMetallic);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, gAO);

        deferredPass.setVec3("viewPos", camPos);
        // finally render quad

        glEnable(GL_SCISSOR_TEST);
        glViewport(0, 0, screenWidth/2, screenHeight);
        glScissor(0, 0, screenWidth/2, screenHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderQuad();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
        // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the 		
        // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
        glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glDisable(GL_DEPTH_TEST);
        decalsPass.use();
        decalsPass.setMat4("model", model);
        decalsPass.setMat4("projection", projection);
        decalsPass.setMat4("view", view);
        decalsPass.setVec2("iResolution", widthHeight);
        decalsPass.setVec3("camPos", camPos);
        decalsPass.setVec3("camDir", camFront);
        decalsPass.setFloat("iTest", test);
        decalsPass.setFloat("iFlip", flip);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rboDepth);
        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, decalTexture);

        glEnable(GL_SCISSOR_TEST);
        glViewport(0, 0, screenWidth/2, screenHeight);
        glScissor(0, 0, screenWidth/2, screenHeight);

        //renderQuad();
        renderCube();
        glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        // Draw texture baker.
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glBindFramebuffer(GL_FRAMEBUFFER, baker);
        
        bakePass.use();
        glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.baseColor);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.roughness);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material.metallic);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, material.ao);

        glEnable(GL_SCISSOR_TEST);
        glViewport(screenWidth/2, 0, screenWidth/2, screenHeight);
        glScissor(screenWidth/2, 0, screenWidth/2, screenHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        SDL_PumpEvents();  // make sure we have the latest mouse state.

        SDL_GL_GetDrawableSize(window, &screenWidth, &screenHeight);

        iTime += 1.0f / 1000.0f;

    };

    emscripten_set_main_loop(main_loop, 0, true);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    return EXIT_SUCCESS;
}
