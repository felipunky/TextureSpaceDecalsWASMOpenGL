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
#include "Include/OpenGLTF.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "nanort.h"

#if 0
#define glGenVertexArrays glGenVertexArraysOES
#define glBindVertexArray glBindVertexArrayOES
#endif

#define PROJECTOR_DISTANCE 50.0f
#define FAR_PLANE 1000.0F
#if 1
    #define PILOT_SHIRT
#endif

const int WIDTH  = 800,
          HEIGHT = 600;

//float* newVertices;
//float* newNormals;
//float* newTextureCoords;
int newVerticesSize;
int flipAlbedo = 0;
uint8_t reload = 0u;
uint8_t downloadImage = 0u;
std::vector<uint8_t> decalImageBuffer,
                     albedoBuffer,
                     roughnessBuffer,
                     normalBuffer,
                     metallicBuffer,
                     ambienOcclusionBuffer;
std::vector<uint8_t> decalResult;
uint16_t widthDecal = 4509u, heightDecal = 2798u, changeDecal = 0u,
         widthAlbedo,        heightAlbedo,        changeAlbedo = 0u;
Shader geometryPass = Shader();

nanort::BVHAccel<float> accel;
unsigned int VBOVertices, VBONormals, VBOTextureCoordinates, VAO, EBO;

std::vector<uint8_t> uploadImage()
{
    unsigned int bufferSize = 4 * geometryPass.Width * geometryPass.Height;
    std::vector<uint8_t> buffer(bufferSize);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, geometryPass.Width, geometryPass.Height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    return buffer;
}

std::vector<uint8_t> loadArray(uint8_t* buf, int bufSize)
{
    std::cout << "Buffer size: " << bufSize << std::endl;
    std::vector<uint8_t> newBuf(bufSize);
    for (int i = 0; i < bufSize; ++i)
    {
        newBuf[i] = buf[i];
    }
    return newBuf;
}

void reloadModel();
void ObjLoader(std::string inputFile);
void loadGLTF(tinygltf::Model &model);

struct Material
{
    unsigned int baseColor;
    unsigned int normal;
    unsigned int roughness;
    unsigned int metallic;
    unsigned int ao;
};

Material material;

bool isGLTF = false;

extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void passGLTF(char* buf)
    {
        std::string result = buf;
        tinygltf::Model model;
        std::vector<unsigned char> data(result.begin(), result.end());
        bool res = GLTF::GetGLTFModel(&model, data);
        if (!res)
        {
            throw std::runtime_error("Unable to read GLTF!");
        }
        loadGLTF(model);
        reloadModel();
        isGLTF = true;
        data.clear();
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passObj(char* buf)
    {
        //std::cout << "New mesh content: " << buf << std::endl;
        std::string result = buf;
        ObjLoader(result);
        reloadModel();
        isGLTF = false;
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void load(uint8_t* buf, int bufSize) 
    {
        //printf("[WASM] Loading Texture \n");
        std::cout << "Reading decal image!" << std::endl;
        std::cout << "Decal buffer size: " << bufSize << std::endl;
        
        decalImageBuffer = loadArray(buf, bufSize);
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void loadAlbedo(uint8_t* buf, int bufSize)
    {
        std::cout << "Reading albedo from file!" << std::endl;
        std::cout << "Albedo buffer size: " << bufSize << std::endl;

        albedoBuffer = loadArray(buf, bufSize);
        geometryPass.createTextureFromFile(&(material.baseColor), buf, geometryPass.Width, geometryPass.Height, "BaseColor", 0);
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passSizeAlbedo(uint16_t* buf, int bufSize)
    {
        std::cout << "Reading decal image size!" << std::endl;
        geometryPass.Width = (int)buf[0];
        geometryPass.Height = (int)buf[1];
        flipAlbedo = (int)buf[2];
        std::cout << "Albedo Width: "  << +geometryPass.Width  << std::endl;
        std::cout << "Albedo Height: " << +geometryPass.Height << std::endl;
        std::cout << "Changed Albedo: "<< +flipAlbedo        << std::endl;
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passSize(uint16_t* buf, int bufSize)
    {
        std::cout << "Reading decal image size!" << std::endl;
        widthDecal  = buf[0];
        heightDecal = buf[1];
        changeDecal = buf[2];
        std::cout << "Decal Width: "  << +widthDecal  << std::endl;
        std::cout << "Decal Height: " << +heightDecal << std::endl;
        std::cout << "Clicked :"      << +changeDecal     << std::endl;
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    uint8_t* downloadDecal(uint8_t *buf, int bufSize) 
    {
        if (decalResult.size() > 0)
        {
            std::cout << "Successful loading the image into data!" << std::endl;
            uint8_t* result = &decalResult[0];
            free(buf);
            return result;
        }
        else
        {
            std::cout << "Unsuccesful loading the image into data!" << std::endl;
            int size = 4 * geometryPass.Width * geometryPass.Height;//*/WIDTH * HEIGHT;
            uint8_t values[size];
    
            for (int i = 0; i < size; i++) 
            {
                values[i] = 0u;
            }
        
            auto arrayPtr = &values[0];
            free(buf);
            return arrayPtr;
        }
    }
    EMSCRIPTEN_KEEPALIVE
    void downloadDecalTrigger(uint8_t* buf, int bufSize)
    {
        downloadImage = 1u;
        free(buf);
    }
}

bool useWindow = false;
float camDistance = 1.f;
static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);

// an example of something we will control from the javascript side

std::function<void()> loop;
void main_loop() { loop(); }

std::vector<glm::vec3> modelDataVertices,
                       meshRawVertices;
std::vector<glm::vec3> modelDataNormals;
std::vector<glm::vec2> modelDataTextureCoordinates;
std::vector<tinyobj::material_t> modelDataMaterial;
std::map<std::string, GLuint> modelDataTextures;
std::vector<unsigned int> indexes,
                          meshRawIndexes;
glm::vec3 bboxMin = glm::vec3(1e+5);
glm::vec3 bboxMax = glm::vec3(-1e+5);
glm::vec3 centroid;

// Projector
glm::vec3 projectorPos;
glm::vec3 projectorDir;
glm::mat4 projectorView;
glm::mat4 projectorProjection;
glm::mat4 decalProjector;
float     projectorSize     = 0.1f;
float     projectorRotation = 0.0f;

int lastUsing = 0;

const glm::vec4 kFrustumCorners[] = {
    glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),  // Far-Bottom-Left
    glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f),   // Far-Top-Left
    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),    // Far-Top-Right
    glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),   // Far-Bottom-Right
    glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), // Near-Bottom-Left
    glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f),  // Near-Top-Left
    glm::vec4(1.0f, 1.0f, -1.0f, 1.0f),   // Near-Top-Right
    glm::vec4(1.0f, -1.0f, -1.0f, 1.0f)   // Near-Bottom-Right
};

void renderLineStrip(glm::vec3* vertices, const int& count)
{
    unsigned int lineStripVAO = 0,
                 lineStripVBO;
    glGenVertexArrays(1, &lineStripVAO);
    glGenBuffers(1, &lineStripVBO);
    glBindVertexArray(lineStripVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineStripVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * count, vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glDrawArrays(GL_LINE_STRIP, 0, count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void renderLine(glm::vec3& A, glm::vec3& B)
{
    glm::vec3 vertices[] = {A, B};
    unsigned int lineVAO = 0,
                 lineVBO;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void renderFrustum(const glm::mat4& view_proj)
{
    glm::mat4 inverse = glm::inverse(view_proj);
    glm::vec3 corners[8];

    for (int i = 0; i < 8; i++)
    {
        glm::vec4 v = inverse * kFrustumCorners[i];
        v           = v / v.w;
        corners[i]  = glm::vec3(v.x, v.y, v.z);
    }

    glm::vec3 _far[5] = { corners[0], corners[1], corners[2], corners[3], corners[0] };

    renderLineStrip(&_far[0], 5);

    glm::vec3 _near[5] = { corners[4], corners[5], corners[6], corners[7], corners[4] };

    renderLineStrip(&_near[0], 5);

    renderLine(corners[0], corners[4]);
    renderLine(corners[1], corners[5]);
    renderLine(corners[2], corners[6]);
    renderLine(corners[3], corners[7]);
}

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
      if (ImGui::IsKeyPressed(ImGuiKey_E)) 
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

void ObjLoader(std::string inputFile)
{
    bboxMin = glm::vec3(1e+5);
    bboxMax = glm::vec3(-1e+5);   
    modelDataVertices.clear();
    modelDataNormals.clear();
    modelDataTextureCoordinates.clear();
    indexes.clear();
    //delete[] mesh.Vertices;
    //delete[] mesh.Normals;
    //delete[] mesh.Faces;

    std::istringstream stream;
    if (inputFile == "Assets/t-shirt-lp/source/Shirt.obj")
    {
        stream = std::istringstream(LoadTextFile(inputFile));
    }

	else
    {
        stream = std::istringstream(inputFile);
    }
    //std::cout << stream.str() << std::endl;
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

    //mesh.NumOfFaces    = indexes.size() / 3;
    //mesh.NumOfVertices = modelDataVertices.size();
    //mesh.Vertices      = glm::value_ptr(modelDataVertices[0]);
    //mesh.Normals       = &modelDataNormals[0][0];
    //mesh.Faces         = &indexes[0];

    std::cout << "Vertices: " << modelDataVertices.size() << "\n";
    std::cout << "Normals: " << modelDataNormals.size() << "\n";
    std::cout << "Texture Coordinates: " << modelDataTextureCoordinates.size() << "\n";
    std::cout << "Materials: " << materials.size() << "\n";
    std::cout << "BboxMax: {x: " << bboxMax.x << ", y: " << bboxMax.y << ", z: " << bboxMax.z << "}\n";
    std::cout << "BboxMin: {x: " << bboxMin.x << ", y: " << bboxMin.y << ", z: " << bboxMin.z << "}\n";
}

bool loadModel(tinygltf::Model &model, const char *filename) {
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
  if (!warn.empty()) {
    std::cout << "WARN: " << warn << std::endl;
  }

  if (!err.empty()) {
    std::cout << "ERR: " << err << std::endl;
  }

  if (!res)
    std::cout << "Failed to load glTF: " << filename << std::endl;
  else
    std::cout << "Loaded glTF: " << filename << std::endl;

  return res;
}

const float* GetDataFromAccessorGLTF(const tinygltf::Model &model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
    // cast to float type read only. Use accessor and bufview byte offsets to determine where position data 
    // is located in the buffer.
    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
    // bufferView byteoffset + accessor byteoffset tells you where the actual position data is within the buffer. From there
    // you should already know how the data needs to be interpreted.
    const float* result = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
    return result;
}

void loadGLTF(tinygltf::Model &model) 
{
    bboxMin = glm::vec3(1e+5);
    bboxMax = glm::vec3(-1e+5);   
    modelDataVertices.clear();
    modelDataNormals.clear();
    modelDataTextureCoordinates.clear();
    indexes.clear();

    std::unordered_map<std::bitset<256>, uint32_t> uniqueVertices;
    uint32_t counter = 0;
    int primCounter = 0;

    std::vector<unsigned int> _u32Buffer;
    std::vector<unsigned short> _u16Buffer;
    std::vector<unsigned char> _u8Buffer;

    for (auto &mesh : model.meshes) 
    {
        std::cout << "mesh : " << mesh.name << std::endl;

        for (const auto& prim : mesh.primitives)
        {

            bool result = GLTF::GetAttributes<glm::vec3>(model, prim, modelDataVertices, "POSITION");
                 result = GLTF::GetAttributes<glm::vec3>(model, prim, modelDataNormals, "NORMAL");
                 result = GLTF::GetAttributes<glm::vec2>(model, prim, modelDataTextureCoordinates, "TEXCOORD_0");

            if (prim.indices > -1)
            {
                const tinygltf::Accessor& indexAccessor = model.accessors[prim.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                switch (indexAccessor.componentType)
                {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    _u32Buffer.resize(indexAccessor.count);
                    std::memcpy(&_u32Buffer[0], &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset], indexAccessor.count * sizeof(unsigned int));
                    indexes.insert(indexes.end(), std::make_move_iterator(_u32Buffer.begin()), std::make_move_iterator(_u32Buffer.end()));
                    break;
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    _u16Buffer.resize(indexAccessor.count);
                    std::memcpy(&_u16Buffer[0], &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset], indexAccessor.count * sizeof(unsigned short));
                    indexes.insert(indexes.end(), std::make_move_iterator(_u16Buffer.begin()), std::make_move_iterator(_u16Buffer.end()));
                    break;
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    _u8Buffer.resize(indexAccessor.count);
                    std::memcpy(&_u8Buffer[0], &buffer.data[indexAccessor.byteOffset + bufferView.byteOffset], indexAccessor.count * sizeof(unsigned char));
                    indexes.insert(indexes.end(), std::make_move_iterator(_u8Buffer.begin()), std::make_move_iterator(_u8Buffer.end()));
                    break;
                default:
                    std::cerr << "Unknown index component type : " << indexAccessor.componentType << " is not supported" << std::endl;
                    return;
                }
            }
            else
            {
                //! Primitive without indices, creating them
                const auto& accessor = model.accessors[prim.attributes.find("POSITION")->second];
                for (unsigned int i = 0; i < accessor.count; ++i)
                    indexes.push_back(i);
            }

        }
    }
    std::cout << "Vertices: " << modelDataVertices.size() << "\n";
    std::cout << "Normals: " << modelDataNormals.size() << "\n";
    std::cout << "Texture Coordinates: " << modelDataTextureCoordinates.size() << "\n";
    //std::cout << "Materials: " << materials.size() << "\n";
    std::cout << "BboxMax: {x: " << bboxMax.x << ", y: " << bboxMax.y << ", z: " << bboxMax.z << "}\n";
    std::cout << "BboxMin: {x: " << bboxMin.x << ", y: " << bboxMin.y << ", z: " << bboxMin.z << "}\n";

    _u8Buffer.clear();
    _u16Buffer.clear();
    _u32Buffer.clear();

    flipAlbedo = 1;
}

void CreateBOs()
{
    // Create a Vertex Buffer Object and copy the vertex data to it
    //unsigned int VBOVertices, VBONormals, VBOTextureCoordinates, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBOVertices);
	glBindBuffer(GL_ARRAY_BUFFER, VBOVertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * modelDataVertices.size(), modelDataVertices.data(), GL_STATIC_DRAW);
    //modelDataVertices.clear();

	glGenBuffers(1, &VBONormals);
	glBindBuffer(GL_ARRAY_BUFFER, VBONormals);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * modelDataNormals.size(), modelDataNormals.data(), GL_STATIC_DRAW);
    //modelDataNormals.clear();

	glGenBuffers(1, &VBOTextureCoordinates);
	glBindBuffer(GL_ARRAY_BUFFER, VBOTextureCoordinates);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * modelDataTextureCoordinates.size(), modelDataTextureCoordinates.data(), GL_STATIC_DRAW);
    //modelDataTextureCoordinates.clear();

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
}

void BuildBVH()
{
    nanort::BVHBuildOptions<float> build_options;  // Use default option
    build_options.cache_bbox = false;

    printf("  BVH build option:\n");
    printf("    # of leaf primitives: %d\n", build_options.min_leaf_primitives);
    printf("    SAH binsize         : %d\n", build_options.bin_size);

    //std::cout << "Vertices size: " << (sizeof(vertices) / sizeof(vertices[0])) << std::endl;

    nanort::TriangleMesh<float> triangle_mesh(glm::value_ptr(modelDataVertices[0]), &indexes[0], sizeof(float) * 3);
    nanort::TriangleSAHPred<float> triangle_pred(glm::value_ptr(modelDataVertices[0]), &indexes[0], sizeof(float) * 3);

    //printf("num_triangles = %zu\n", modelDataVertices.size() / 3);
    //printf("faces = %p\n", indexes.size());

    accel = nanort::BVHAccel<float>();

    nanort::BVHAccel<float> accelDummy;
    bool ret = accelDummy.Build(indexes.size() / 3, triangle_mesh, triangle_pred, build_options);
    assert(ret);

    nanort::BVHBuildStatistics stats = accelDummy.GetStatistics();

    printf("  BVH statistics:\n");
    printf("    # of leaf   nodes: %d\n", stats.num_leaf_nodes);
    printf("    # of branch nodes: %d\n", stats.num_branch_nodes);
    printf("  Max tree depth     : %d\n", stats.max_tree_depth);
    float bmin[3], bmax[3];
    accelDummy.BoundingBox(bmin, bmax);
    printf("  Bmin               : %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
    printf("  Bmax               : %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);
    accel = accelDummy;
}

void reloadModel()
{
    BuildBVH();
    CreateBOs();
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

    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,16);
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
    geometryPass         = Shader("Shaders/GBuffer.vert",      "Shaders/GBuffer.frag");   
    Shader deferredPass  = Shader("Shaders/DeferredPass.vert", "Shaders/DeferredPass.frag");
    Shader hitPosition   = Shader("Shaders/HitPosition.vert",  "Shaders/HitPosition.frag");
    Shader decalsPass    = Shader("Shaders/Decals.vert",       "Shaders/Decals.frag");
    
    //ObjLoader("Assets/t-shirt-lp/source/Shirt.obj");

    #ifdef PILOT_SHIRT
    std::string fileName = "Assets/Pilot/shirt_1.gltf";
    #else
    std::string fileName = "Assets/t-shirt-lp/source/Shirt.gltf";
    #endif
    /**
     * Start Read GLTF
     */
    tinygltf::Model modelGLTF;
    if (!loadModel(modelGLTF, fileName.c_str()))
    {
        throw std::runtime_error("load GLTF Error!");
    }

    loadGLTF(modelGLTF);
    /**
     * End Read GLFT
     */

    geometryPass.use();

    #ifdef PILOT_SHIRT

    geometryPass.createTexture(&(material.normal), "Assets/t-shirt-lp/textures/T_shirt_normal.png", "Normal", 1);
    geometryPass.createTexture(&(material.roughness), "Assets/t-shirt-lp/textures/T_shirt_roughness.jpg", "Roughness", 2);
    geometryPass.createTexture(&(material.metallic), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_metallic.png", "Metallic", 3);
    geometryPass.createTexture(&(material.ao), "Assets/t-shirt-lp/textures/T_shirt_AO.jpg", "AmbientOcclussion", 4);
    geometryPass.createTexture(&(material.baseColor), "Assets/Pilot/textures/T_DefaultMaterial_B.jpg", "BaseColor", 0);

    #else

    geometryPass.createTexture(&(material.normal), "Assets/t-shirt-lp/textures/T_shirt_normal.png", "Normal", 1);
    geometryPass.createTexture(&(material.roughness), "Assets/t-shirt-lp/textures/T_shirt_roughness.jpg", "Roughness", 2);
    geometryPass.createTexture(&(material.metallic), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_metallic.png", "Metallic", 3);
    geometryPass.createTexture(&(material.ao), "Assets/t-shirt-lp/textures/T_shirt_AO.jpg", "AmbientOcclussion", 4);
    geometryPass.createTexture(&(material.baseColor), "Assets/t-shirt-lp/textures/T_shirt_albedo.jpg", "BaseColor", 0);

    #endif

    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int rboDepth;

    // create and attach depth buffer (renderbuffer)
    glGenTextures(1, &rboDepth);
    glBindTexture(GL_TEXTURE_2D, rboDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, WIDTH/4, HEIGHT/4, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, rboDepth, 0);

    unsigned int attachments[1] = {GL_NONE};
    glDrawBuffers(1, attachments);

    // Render to texture space.
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    geometryPass.use();
    std::cout << "Albedo Width: "  << geometryPass.Width  << std::endl;
    std::cout << "Albedo Height: " << geometryPass.Height << std::endl;

    unsigned int render;
    glGenTextures(1, &render);
    glBindTexture(GL_TEXTURE_2D, render);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geometryPass.Width, geometryPass.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render, 0);

    // Set the list of draw buffers.
    unsigned int drawBuffersFBO[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffersFBO); // "1" is the size of DrawBuffers

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    deferredPass.use();
    deferredPass.setInt("gNormal",    0);
    deferredPass.setInt("gAlbedo",    1);
    deferredPass.setInt("gMetallic",  2);
    deferredPass.setInt("gRoughness", 3);
    deferredPass.setInt("gAO",        4);

    /**
     * End Shader Setup
     */

    /**
     * Start Geometry Definition
     */

    // Create a Vertex Buffer Object and copy the vertex data to it
    CreateBOs();

    glEnable(GL_DEPTH_TEST); 
    /**
     * End Geometry Definition
     */

    bool showProjector = false;
    bool showHitPoint  = false;
    
    float scale = 1.0f;
    float blend = 0.5f;
	bool insideImGui = false;
    
	// Create the camera (eye).
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 model = glm::mat4(1.0f);
    centroid = bboxMax * glm::vec3(0.5) + bboxMin * glm::vec3(0.5);

    float iTime = 0.0f;

    unsigned int decalTexture = -1;
    decalsPass.createTexture(&decalTexture, "Assets/Textures/Batman.png", "iChannel0", 1);
    decalsPass.setInt("iChannel1", 0);
    decalsPass.setInt("iDepth", 2);

    int flip = 0;

    int click = 0;

    int halfWidth = WIDTH/2, halfHeight = HEIGHT/2;

    // Build the acceleration structure for ray tracing.
    BuildBVH();

    glm::vec3 hitPos = glm::vec3(0.0, 0.0, 0.0);
    glm::vec3 hitNor = glm::vec3(1.0, 1.0, 1.0);

    unsigned int frame = 0,
                 counter = 0,
                 flipper = 0;

    float zoomSide = 0.5f,
          zoomTop  = zoomSide;

    model = glm::mat4(1.0f);
    model = glm::translate(model, -centroid);

    glm::mat4 modelNoGuizmo = model;

    loop = [&]
    {
        if (decalImageBuffer.size() > 0)
        {
            //decalsPass.createTexture(&decalTexture, "Assets/Textures/WatchMen.jpeg", "iChannel0", 1);
            //decalsPass.createTextureFromFile(&decalTexture, decalImageBuffer, widthDecal, heightDecal, "iChannel0", 1);
            std::cout << "Changing texture" << std::endl;
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
            glTexImage2D(GL_TEXTURE_2D, 0, format, widthDecal, heightDecal, 0, format, GL_UNSIGNED_BYTE, decalImageBuffer.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            // Clear the data.
            decalImageBuffer.clear();

            // Bind the uniform sampler.
            decalsPass.use();
            decalsPass.setInt("iChannel0", 1);
        }

        /**
         * Start ImGui
         */	        
        float camSpeed = deltaTime * SPEED;
        bool shiftIsPressed = false;
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

        view = glm::lookAt(camPos, camPos + camFront, camUp);

        //std::cout << glm::to_string(view) << std::endl;

        glm::vec2 widthHeight = glm::vec2(screenWidth, screenHeight);

        // No need to compute this every frame as the FOV stays always the same.
        glm::vec2 halfWidthHeight = widthHeight * glm::vec2(0.5, 0.5);
        float aspect = halfWidthHeight.x / halfWidthHeight.y;
        float near = 0.1f;
        float far = 200.0f;

		glm::mat4 projection     = glm::perspective( glm::radians( 45.0f ), widthHeight.x / widthHeight.y, near, far);
        glm::mat4 projectionHalf = glm::perspective( glm::radians( 45.0f ), halfWidthHeight.x / widthHeight.y, near, far);                                       
        glm::mat4 projectionSide = glm::ortho(-aspect, aspect, -1.0f, 1.0f, near, far);
        glm::vec3 mouse;
        if (!insideImGui)
		{
            //int x, y;
            Uint32 buttons;
            buttons = SDL_GetMouseState(&mousePositionX, &mousePositionY);
            mouse = glm::vec3(mousePositionX, -mousePositionY + screenHeight, (clicked ? 1.0f : 0.0f));
            mousePositionX = mousePositionX + screenWidth / 4;
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

            if ((buttons & SDL_BUTTON_RMASK) != 0)
            {
                std::cout << "Right button pressed!" << std::endl;
                glm::vec4 NDCPos      = glm::vec4((2.0f * float(mousePositionX)) / widthHeight.x - 1.0f, 
                                                   1.0f - (2.0f * float(mousePositionY)) / widthHeight.y, 
                                                   1.0f, 
                                                   1.0f);
                glm::mat4 inverseModel = glm::inverse(model);
                glm::vec4 viewCoords   = glm::inverse(projection) * NDCPos;
                viewCoords             = glm::inverse(view) * viewCoords;
                glm::vec4 worldCoords  = inverseModel * glm::vec4(viewCoords.x, viewCoords.y, -1.0f, 0.0f);

                glm::vec3 rayDir = glm::normalize(glm::vec3(worldCoords));
                glm::vec3 rayOri = glm::vec3(inverseModel * glm::vec4(camPos.x, camPos.y, camPos.z, 1.0f));

                nanort::Ray<float> ray;
                ray.org[0] = rayOri.x;
                ray.org[1] = rayOri.y;
                ray.org[2] = rayOri.z;

                ray.dir[0] = rayDir.x;
                ray.dir[1] = rayDir.y;
                ray.dir[2] = rayDir.z;

                float kFar = 1.0e+30f;
                ray.min_t = 0.0f;
                ray.max_t = kFar;

                nanort::TriangleIntersector<> triangle_intersector(glm::value_ptr(modelDataVertices[0]), &indexes[0], sizeof(float) * 3);
                nanort::TriangleIntersection<> isect;
                bool hit = accel.Traverse(ray, triangle_intersector, &isect);
                if (hit)
                {
                    hitPos = rayOri + rayDir * isect.t;
                    
                    unsigned int fid = isect.prim_id;
                    unsigned int id  = indexes[3 * fid];
                    hitNor = modelDataNormals[id];

                    projectorPos = hitPos + hitNor * PROJECTOR_DISTANCE;
                    projectorDir = -hitNor;

                    float ratio = float(heightDecal) / float(widthDecal);
                    float proportionateHeight = projectorSize * ratio;

                    glm::mat4 rotate = glm::mat4(1.0f);
                    rotate = glm::rotate(rotate, glm::radians(projectorRotation), projectorDir);

                    glm::vec4 axis      = rotate * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
                    projectorView       = glm::lookAt(projectorPos, hitPos, axis.xyz());
                    projectorProjection = glm::ortho(-projectorSize, projectorSize, -proportionateHeight, proportionateHeight, 0.001f, FAR_PLANE);

                    decalProjector = projectorProjection * projectorView;

                    printf("Hit Point: %s\n", glm::to_string(hitPos).c_str());
                    printf("Hit Normal: %s\n", glm::to_string(hitNor).c_str());
                    printf("Decal Projector: %s\n", glm::to_string(decalProjector).c_str());
                }
            }

		}

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        ImGui::Begin("Graphical User Interface");   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        std::string printTime = std::to_string(deltaTime * 1000.0f) + " ms.\n";
		ImGui::Text(printTime.c_str());
        ImGui::SliderFloat("Texture Coordinates Scale", &scale, 1.0f, 100.0f);
        ImGui::SliderFloat("Blend Factor", &blend, 0.0f, 1.0f);
        ImGui::SliderFloat("Projector Size", &projectorSize, 0.1f, 1.0f);
        ImGui::SliderFloat("Projector Orientation", &projectorRotation, 0.0f, 360.0f); 
        if (ImGui::Button("Show Projector"))      
        {
            showProjector = !showProjector;
        }
        if (ImGui::Button("Show Hit Point"))
        {
            showHitPoint = !showHitPoint;
        }
        ImGui::SliderFloat("Side Zoom", &zoomSide, 0.1f, 2.0f);
        ImGui::SliderFloat("Front Zoom", &zoomTop, 0.1f, 2.0f);
        // If we have a GLTF we need to invert the texture coordinates.
        flipper = isGLTF ? 1 : 0;
        decalsPass.use();
        decalsPass.setInt("iFlipper", flipper);

        for (int i = 0; i < 1; ++i)
        {
            ImGuizmo::SetID(i);
            EditTransform(glm::value_ptr(view), glm::value_ptr(projectionHalf), glm::value_ptr(model), lastUsing == i, io);
            if (ImGuizmo::IsUsing())
            {
                lastUsing = i;
            }
        }
        
        ImGui::End();

        // Calculate the time between frames.
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        deltaTime = time - lastFrame;
        lastFrame = time;

        // render
        // ------
        // Depth pre-pass.
        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer); 
        
        geometryPass.use();
        glm::mat4 mm = glm::mat4(1.0f);
        geometryPass.setMat4("model", mm);
        geometryPass.setMat4("projection", projection);
        geometryPass.setMat4("view", view);
        geometryPass.setMat4("decalProjector", decalProjector);
        geometryPass.setFloat("flipAlbedo", (float)flipAlbedo);
        glBindVertexArray(VAO); 
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

        glViewport(0, 0, WIDTH/4, HEIGHT/4);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        decalsPass.use();
        decalsPass.setMat4("model", model);
        decalsPass.setMat4("projection", projection);
        decalsPass.setMat4("view", view);
        decalsPass.setVec2("iResolution", widthHeight);
        decalsPass.setFloat("iScale", scale);
        decalsPass.setFloat("iFlip", (float)flip);
        decalsPass.setFloat("iBlend", blend);
        decalsPass.setMat4("decalProjector", decalProjector);
        decalsPass.setFloat("iFlipAlbedo", (float)flipAlbedo);
        //decalsPass.setFloat("iFlipAlbedo", (float)flipAlbedo);

        glBindVertexArray(VAO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.baseColor);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, decalTexture);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, rboDepth);

        glViewport(0, 0, geometryPass.Width, geometryPass.Height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        if (downloadImage == 1 || frame == 0u)
        {
            decalResult = uploadImage();
            // counter = 0;
            downloadImage = 0u;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
        glCullFace(GL_BACK);

        // Light pass.
        //glScissor(0, 0, screenWidth/2, screenHeight);
        glViewport(0, 0, screenWidth/2, screenHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(VAO);
        deferredPass.use();
        deferredPass.setMat4("model", model);
        deferredPass.setMat4("projection", projectionHalf);
        deferredPass.setMat4("view", view);
        deferredPass.setFloat("iFlipAlbedo", (float)flipAlbedo);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, render);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.metallic);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material.ao);

        deferredPass.setVec3("viewPos", camPos);
        deferredPass.setFloat("iTime", iTime);
        // finally render quad
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        
        // renderQuad();

        // glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
        // // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the 		
        // // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
        // glBlitFramebuffer(0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        
        if (showHitPoint)
        {
            glm::mat4 hitPositionModel = model;
            hitPositionModel = glm::translate(hitPositionModel, hitPos);
            hitPositionModel = glm::scale(hitPositionModel, glm::vec3(0.01));

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glEnable(GL_DEPTH_TEST);

            hitPosition.use();
            hitPosition.setMat4("model",      hitPositionModel);
            hitPosition.setMat4("projection", projectionHalf);
            hitPosition.setMat4("view",       view);
            renderCube();
        }

        if (showProjector)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glEnable(GL_DEPTH_TEST);

            hitPosition.use();
            hitPosition.setMat4("model",      model);
            hitPosition.setMat4("projection", projectionHalf);
            hitPosition.setMat4("view",       view);
            renderFrustum(decalProjector);
        }

        // Side View.
        glViewport(screenWidth/2, 0, screenWidth/2, screenHeight/2);

        // Scale for the side view.
        glm::mat4 modelSide = glm::scale(modelNoGuizmo, glm::vec3(zoomSide, zoomSide, zoomSide));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(VAO);
        deferredPass.use();
        deferredPass.setMat4("model", modelSide);
        deferredPass.setMat4("projection", projectionSide);
        deferredPass.setMat4("view", glm::mat4(-0.113205, -0.187880, 0.975646, 0.000000, 
                                               0.000000, 0.981959, 0.189095, 0.000000, 
                                               -0.993572, 0.021407, -0.111162, 0.000000, 
                                               -0.064962, 0.388481, -4.221102, 1.000000));
        deferredPass.setFloat("iFlipAlbedo", (float)flipAlbedo);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, render);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.metallic);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material.ao);

        deferredPass.setVec3("viewPos", camPos);
        deferredPass.setFloat("iTime", iTime);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        // Front View.
        glViewport(screenWidth/2, screenHeight/2, screenWidth/2, screenHeight/2);

        // Scale for the side view.
        glm::mat4 modelTop = glm::scale(modelNoGuizmo, glm::vec3(zoomTop, zoomTop, zoomTop));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(VAO);
        deferredPass.use();
        deferredPass.setMat4("model", modelTop);
        deferredPass.setMat4("projection", projectionSide);
        deferredPass.setMat4("view", glm::mat4(1.0, 0.0,  0.0, 0.0, 
                                               0.0, 1.0,  0.0, 0.0, 
                                               0.0, 0.0,  1.0, 0.0, 
                                               0.0, 0.0, -1.0, 1.0));
        deferredPass.setFloat("iFlipAlbedo", (float)flipAlbedo);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, render);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.metallic);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material.ao);

        deferredPass.setVec3("viewPos", camPos);
        deferredPass.setFloat("iTime", iTime);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        SDL_PumpEvents();  // make sure we have the lascale mouse state.

        SDL_GL_GetDrawableSize(window, &screenWidth, &screenHeight);

        iTime += 1.0f / 1000.0f;
        frame++;
        counter++;
        changeDecal = 0u;
    };

    emscripten_set_main_loop(main_loop, 0, true);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    modelDataNormals.clear();
    indexes.clear();
    //delete[] mesh.Vertices;
    //delete[] mesh.Normals;
    //delete[] mesh.Faces;
    //delete newVertices;
    //delete newNormals;
    //delete newTextureCoords;

    return EXIT_SUCCESS;
}
