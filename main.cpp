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
#define FAR_PLANE  1000.0f
#define NEAR_PLANE 1e-5
#define RADIANS_30 glm::radians(30.0f)
#define RADIANS_45 glm::radians(45.0f)
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
std::vector<uint8_t> decalImageBuffer;
std::vector<uint8_t> decalResult;
uint16_t widthDecal = 1127u, heightDecal = 699u, changeDecal = 0u,
         widthAlbedo,        heightAlbedo,        changeAlbedo = 0u;
Shader geometryPass = Shader();
unsigned int renderAlbedo;
unsigned int fbo;

float near          = 0.1f;
float far           = 200.0f;
float focalDistance = 3.0f;
float radius        = 1.0f;

glm::vec3 bboxMin  = glm::vec3(1e+5);
glm::vec3 bboxMax  = glm::vec3(-1e+5);
glm::vec3 centroid = glm::vec3(-1e+5);
glm::vec3 camPos;
glm::vec3 camFront = glm::vec3( 0.0f, 0.0f, -1.0f );
glm::vec3 camUp = glm::vec3( 0.0f, 1.0f, 0.0f );
glm::vec3 differenceBboxMaxMin;

glm::mat4 model = glm::mat4(1.0f);

nanort::BVHAccel<float> accel;
unsigned int VBOVertices, VBONormals, VBOTextureCoordinates, VBOTangents, VAO, EBO;

std::vector<uint8_t> uploadImage()
{
    unsigned int bufferSize = 4 * geometryPass.Width * geometryPass.Height;
    std::vector<uint8_t> buffer(bufferSize);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, geometryPass.Width, geometryPass.Height, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    return buffer;
}

std::vector<uint8_t> loadArray(uint8_t* buf, int bufSize)
{
    #ifdef OPTIMIZE
    #else
    std::cout << "Buffer size: " << bufSize << std::endl;
    #endif
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
void recomputeCamera();

struct Material
{
    unsigned int baseColor;
    unsigned int normal;
    unsigned int roughness;
    unsigned int metallic;
    unsigned int ao;
};

void clearBBox(glm::vec3& bboxMin, glm::vec3& bboxMax, glm::vec3& centroid)
{
    bboxMin  = glm::vec3(1e+5);
    bboxMax  = glm::vec3(-1e+5);
    centroid = glm::vec3(-1e+5);
}

Material material;

bool isGLTF = false;

extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void passGLTF(char* buf)
    {
        //clearBBox(bboxMin, bboxMax, centroid);
        std::string result = buf;
        tinygltf::Model model;
        std::vector<unsigned char> data(result.begin(), result.end());
        bool res = GLTF::GetGLTFModel(&model, data);
        if (!res)
        {
            #ifdef EXCEPTIONS
            throw std::runtime_error("Unable to read GLTF!");
            #else
            std::cout << "Unable to read GLTF!" << std::endl;
            #endif
        }
        loadGLTF(model);
        reloadModel();
        recomputeCamera();
        isGLTF = true;
        data.clear();
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passObj(char* buf)
    {
        //std::cout << "New mesh content: " << buf << std::endl;
        //clearBBox(bboxMin, bboxMax, centroid);
        std::cout << "BBox Cleared Center: {x: " << centroid.x << ", y: " << centroid.y << ", z:" << centroid.z << "}\n";
        std::string result = buf;
        ObjLoader(result);
        reloadModel();
        recomputeCamera();
        isGLTF = false;
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void load(uint8_t* buf, int bufSize) 
    {
        //printf("[WASM] Loading Texture \n");
        #ifdef OPTIMIZE
        #else
        std::cout << "Reading decal image!" << std::endl;
        std::cout << "Decal buffer size: " << bufSize << std::endl;
        #endif
        decalImageBuffer = loadArray(buf, bufSize);
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void loadAlbedo(uint8_t* buf, int bufSize)
    {
        #ifdef OPTIMIZE
        #else
        std::cout << "Reading albedo from file!" << std::endl;
        std::cout << "Albedo buffer size: " << bufSize << std::endl;
        #endif
        geometryPass.createTextureFromFile(&(material.baseColor), buf, geometryPass.Width, geometryPass.Height, "BaseColor", 0);
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passSizeAlbedo(uint16_t* buf, int bufSize)
    {
        #ifdef OPTIMIZE
        #else
        std::cout << "Reading Albedo image size!" << std::endl;
        #endif
        int width  = (int)buf[0];
        int height = (int)buf[1];

        geometryPass.use();
        geometryPass.Width  = width;
        geometryPass.Height = height;
        flipAlbedo = (int)buf[2];
        
        #ifdef OPTIMIZE
        #else
        std::cout << "Albedo size changed regenerating glTexImage2D" << std::endl;
        #endif
        glDeleteTextures(1, &renderAlbedo);
        glDeleteTextures(1, &(material.baseColor));

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &renderAlbedo);
        glBindTexture(GL_TEXTURE_2D, renderAlbedo);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geometryPass.Width, geometryPass.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderAlbedo, 0);
        
        #ifdef OPTIMIZE
        #else
        std::cout << "Albedo Width: "  << +geometryPass.Width  << std::endl;
        std::cout << "Albedo Height: " << +geometryPass.Height << std::endl;
        std::cout << "Changed Albedo: "<< +flipAlbedo        << std::endl;
        #endif
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void loadNormal(uint8_t* buf, int bufSize)
    {
        #ifdef OPTIMIZE
        #else
        std::cout << "Reading normal from file!" << std::endl;
        std::cout << "Normal buffer size: " << bufSize << std::endl;
        #endif
        geometryPass.createTextureFromFile(&(material.normal), buf, geometryPass.Width, geometryPass.Height, "BaseColor", 0);
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passSizeNormal(uint16_t* buf, int bufSize)
    {
        #ifdef OPTIMIZE
        #else
        std::cout << "Reading Normal image size!" << std::endl;
        #endif
        int width  = (int)buf[0];
        int height = (int)buf[1];

        geometryPass.use();

        #ifdef OPTIMIZE
        #else
        std::cout << "Normal size changed regenerating glTexImage2D" << std::endl;
        #endif
        glDeleteTextures(1, &renderAlbedo);
        glDeleteTextures(1, &(material.normal));

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &renderAlbedo);
        glBindTexture(GL_TEXTURE_2D, renderAlbedo);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geometryPass.Width, geometryPass.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderAlbedo, 0);

        geometryPass.Width  = width;
        geometryPass.Height = height;
        #ifdef OPTIMIZE
        #else
        std::cout << "Normal Width: "  << +geometryPass.Width  << std::endl;
        std::cout << "Normal Height: " << +geometryPass.Height << std::endl;
        #endif
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    void passSize(uint16_t* buf, int bufSize)
    {
        #ifdef OPTIMIZE
        #else
        std::cout << "Reading decal image size!" << std::endl;
        #endif
        widthDecal  = buf[0];
        heightDecal = buf[1];
        changeDecal = buf[2];
        #ifdef OPTIMIZE
        #else
        std::cout << "Decal Width: "  << +widthDecal  << std::endl;
        std::cout << "Decal Height: " << +heightDecal << std::endl;
        std::cout << "Clicked :"      << +changeDecal     << std::endl;
        #endif
        free(buf);
    }
    EMSCRIPTEN_KEEPALIVE
    uint8_t* downloadDecal(uint8_t *buf, int bufSize) 
    {
        if (decalResult.size() > 0)
        {
            #ifdef OPTIMIZE
            #else
            std::cout << "Successful loading the image into data!" << std::endl;
            #endif
            uint8_t* result = &decalResult[0];
            free(buf);
            return result;
        }
        else
        {
            #ifdef OPTIMIZE
            #else
            std::cout << "Unsuccesful loading the image into data!" << std::endl;
            #endif
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

std::vector<glm::vec3> modelDataVertices;
std::vector<glm::vec3> modelDataNormals;
std::vector<glm::vec2> modelDataTextureCoordinates;
std::vector<glm::vec4> modelDataTangents;
std::vector<tinyobj::material_t> modelDataMaterial;
std::map<std::string, GLuint> modelDataTextures;
std::vector<unsigned int> indexes;

// Projector
glm::vec3 projectorPos;
glm::vec3 projectorDir;
// glm::mat4 projectorView;
// glm::mat4 projectorProjection;
glm::mat4 decalProjector;
float     projectorSize     = 0.5f;
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

std::array<glm::vec3, 8> cubeCorners(const glm::vec3& min, const glm::vec3& max) 
{
    std::array<glm::vec3, 8> kCubeCorners = {
        glm::vec3(min.x, min.y, max.z),  // Far-Bottom-Left   
        glm::vec3(min.x, max.y, max.z),  // Far-Top-Left      
        max,                             // Far-Top-Right     
        glm::vec3(max.x, min.y, max.z),  // Far-Bottom-Right  
        min,                             // Near-Bottom-Left  
        glm::vec3(min.x, max.y, min.z),  // Near-Top-Left     
        glm::vec3(max.x, max.y, min.z),  // Near-Top-Right    
        glm::vec3(max.x, min.y, min.z)   // Near-Bottom-Right 
    };
    return kCubeCorners;
}

void rayTrace(const int& mousePositionX, const int& mousePositionY, const glm::vec2& widthHeight, 
              const std::vector<glm::vec3>& modelDataVertices, const std::vector<unsigned int>& indexes, 
              const glm::mat4& projection, const glm::mat4& view, const glm::mat4& model, const bool& debug,
              /** Out **/
              glm::vec3& hitNor, glm::vec3& hitPos, glm::mat4& decalProjector)
{
    #ifdef OPTIMIZE
    #else
    std::cout << "Right button pressed!" << std::endl;
    #endif
    
    // https://stackoverflow.com/questions/53467077/opengl-ray-tracing-using-inverse-transformations
    glm::vec2 normalizedMouse = glm::vec2(2.0f, 2.0f) * glm::vec2(mousePositionX, mousePositionY) / widthHeight;
    float x_ndc = normalizedMouse.x - 1.0;
    float y_ndc = 1.0 - normalizedMouse.y; // flipped

    glm::vec4 p_near_ndc = glm::vec4(x_ndc, y_ndc, -1.0f, 1.0f); // z near = -1
    glm::vec4 p_far_ndc  = glm::vec4(x_ndc, y_ndc,  1.0f, 1.0f); // z far = 1

    glm::mat4 invMVP = glm::inverse(projection * view * model);

    glm::vec4 p_near_h = invMVP * p_near_ndc;
    glm::vec4 p_far_h  = invMVP * p_far_ndc;

    glm::vec3 p0 = glm::vec3(p_near_h) / glm::vec3(p_near_h.w, p_near_h.w, p_near_h.w);
    glm::vec3 p1 = glm::vec3(p_far_h)  / glm::vec3(p_far_h.w, p_far_h.w, p_far_h.w);

    glm::vec3 rayOri = p0;
    glm::vec3 rayDir = glm::normalize(p1 - p0);

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

        glm::vec4 axis                = rotate * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        glm::mat4 projectorView       = glm::lookAt(projectorPos, hitPos, axis.xyz());
        glm::mat4 projectorProjection = glm::ortho(-projectorSize, projectorSize, -proportionateHeight, proportionateHeight, 0.001f, FAR_PLANE);

        decalProjector = projectorProjection * projectorView;
        if (debug)
        {
            printf("Hit Point: %s\n", glm::to_string(hitPos).c_str());
            printf("Hit Normal: %s\n", glm::to_string(hitNor).c_str());
            printf("Decal Projector: %s\n", glm::to_string(decalProjector).c_str());
        }
    }
}

// Returns near .x far .y
glm::vec3 calculateNearFarPlane()
{
    // https://community.khronos.org/t/automatically-center-3d-object/20892/6
    glm::vec3 nearFarPlaneBSphere = bboxMax - centroid;
    float r = glm::length(nearFarPlaneBSphere);
    //float r = glm::dot(nearFarPlaneBSphere, nearFarPlaneBSphere);

    float fDistance = r / 0.57735f; // where 0.57735f is tan(30 degrees)
    // The near and far clipping planes then lay either side of this point, allwoing for the radius of the sphere. So -
    // dNear = fDistance - r;
    // dFar  = fDistance + r;
    glm::vec2 result = glm::vec2(fDistance, fDistance) + glm::vec2(-r, r);
    near          = result.x;
    far           = result.y;
    focalDistance = fDistance;
    radius        = r;
    return nearFarPlaneBSphere;
}

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

void renderLineCube(const glm::vec3& min, const glm::vec3& max)
{
    std::array<glm::vec3, 8> corners = cubeCorners(min, max);
    
    glm::vec3 _far[5] = { corners[0], corners[1], corners[2], corners[3], corners[0] };
    renderLineStrip(&_far[0], 5);

    glm::vec3 _near[5] = { corners[4], corners[5], corners[6], corners[7], corners[4] };
    renderLineStrip(&_near[0], 5);

    renderLine(corners[0], corners[4]);
    renderLine(corners[1], corners[5]);
    renderLine(corners[2], corners[6]);
    renderLine(corners[3], corners[7]);
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
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x/2, io.DisplaySize.y);
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
            default:
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
      ImGuizmo::SetRect(0, 0, io.DisplaySize.x/2, io.DisplaySize.y);
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

void ComputeTangents() 
{
    modelDataTangents.clear();
    modelDataTangents.resize(indexes.size(), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    for(unsigned int i = 0; i < indexes.size(); i += 3)
    {

        glm::vec3 vertex0 = modelDataVertices[indexes[i]];
        glm::vec3 vertex1 = modelDataVertices[indexes[i + 1]];
        glm::vec3 vertex2 = modelDataVertices[indexes[i + 2]];


        glm::vec3 deltaPos;
        if(vertex0 == vertex1)
        {
            deltaPos = vertex2 - vertex0;
        }
        else
        {
            deltaPos = vertex1 - vertex0;
        }
        glm::vec2 uv0 = modelDataTextureCoordinates[indexes[i]];
        glm::vec2 uv1 = modelDataTextureCoordinates[indexes[i + 1]];
        glm::vec2 uv2 = modelDataTextureCoordinates[indexes[i + 2]];

        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        glm::vec3 tan; // tangents

        // avoid divion by 0
        if(deltaUV1.s != 0)
        {
            tan = deltaPos / deltaUV1.s;
        }
        else
        {
            tan = deltaPos / 1.0f;
        }
        glm::vec3 normal = modelDataNormals[indexes[i]];
        
        glm::vec3 vectorOne = vertex1 - vertex0;
        glm::vec3 vectorTwo = vertex2 - vertex0;

        float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        glm::vec3 tan2 = glm::vec3((deltaUV1.x * vectorTwo.x - deltaUV2.x * vectorOne.x) * r, 
                                   (deltaUV1.x * vectorTwo.y - deltaUV2.x * vectorOne.y) * r,
                                   (deltaUV1.x * vectorTwo.z - deltaUV2.x * vectorOne.z) * r);
        
        tan = glm::normalize(tan - glm::dot(normal, tan) * normal);
        glm::vec4 tangent = glm::vec4(tan, (glm::dot(glm::cross(normal, tan), tan2) < 0.0f ? -1.0f : 1.0f));

        // write into array - for each vertex of the face the same value
        modelDataTangents[indexes[i]]     = tangent;
        modelDataTangents[indexes[i + 1]] = tangent;
        modelDataTangents[indexes[i + 2]] = tangent;
    }
}

void ObjLoader(std::string inputFile)
{
    bboxMin = glm::vec3(1e+5);
    bboxMax = glm::vec3(-1e+5);   
    modelDataVertices.clear();
    modelDataNormals.clear();
    modelDataTextureCoordinates.clear();
    modelDataTangents.clear();
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
        #ifdef EXCEPTIONS
        throw std::runtime_error("loadOBJFile: Error: " + warning + error);
        #else
        std::cout << "loadOBJFile: Error: " << warning << error << std::endl;
        #endif
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
    centroid = (bboxMin + bboxMax) * 0.5f;
    ComputeTangents();

    std::cout << "BboxMax: {x: " << bboxMax.x << ", y: " << bboxMax.y << ", z: " << bboxMax.z << "}\n";
    std::cout << "BboxMin: {x: " << bboxMin.x << ", y: " << bboxMin.y << ", z: " << bboxMin.z << "}\n";

    #ifdef OPTIMIZE
    #else
    std::cout << "Vertices: " << modelDataVertices.size() << "\n";
    std::cout << "Normals: " << modelDataNormals.size() << "\n";
    std::cout << "Tangents: " << modelDataTangents.size() << "\n";
    std::cout << "Texture Coordinates: " << modelDataTextureCoordinates.size() << "\n";
    std::cout << "Materials: " << materials.size() << "\n";
    std::cout << "BboxMax: {x: " << bboxMax.x << ", y: " << bboxMax.y << ", z: " << bboxMax.z << "}\n";
    std::cout << "BboxMin: {x: " << bboxMin.x << ", y: " << bboxMin.y << ", z: " << bboxMin.z << "}\n";
    #endif
}

bool loadModel(tinygltf::Model &model, const char *filename) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) 
    {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) 
    {
        std::cout << "ERR: " << err << std::endl;
    }

    if (!res)
    {
        std::cout << "Failed to load glTF: " << filename << std::endl;
    }
    #ifdef OPTIMIZE
    #else
    else
    {
        std::cout << "Loaded glTF: " << filename << std::endl;
    }
    #endif
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
    modelDataTangents.clear();
    indexes.clear();

    std::unordered_map<std::bitset<256>, uint32_t> uniqueVertices;
    uint32_t counter = 0;
    int primCounter = 0;

    std::vector<unsigned int> _u32Buffer;
    std::vector<unsigned short> _u16Buffer;
    std::vector<unsigned char> _u8Buffer;

    for (auto &mesh : model.meshes) 
    {
        #ifdef OPTIMIZE
        #else
        std::cout << "mesh : " << mesh.name << std::endl;
        #endif
        for (const auto& prim : mesh.primitives)
        {

            bool result = GLTF::GetAttributes<glm::vec3>(model, prim, modelDataVertices, "POSITION");
                 result = GLTF::GetAttributes<glm::vec3>(model, prim, modelDataNormals, "NORMAL");
                 result = GLTF::GetAttributes<glm::vec2>(model, prim, modelDataTextureCoordinates, "TEXCOORD_0");
                 result = GLTF::GetAttributes<glm::vec4>(model, prim, modelDataTangents, "TANGENT");

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

    if (modelDataTangents.size() == 0)
    {
        ComputeTangents();
    }

    #ifdef OPTIMIZE
    #else
    std::cout << "Vertices: " << modelDataVertices.size() << "\n";
    std::cout << "Normals: " << modelDataNormals.size() << "\n";
    std::cout << "Texture Coordinates: " << modelDataTextureCoordinates.size() << "\n";
    std::cout << "Tangents: " << modelDataTangents.size() << "\n";
    //std::cout << "Materials: " << materials.size() << "\n";
    std::cout << "BboxMax: {x: " << bboxMax.x << ", y: " << bboxMax.y << ", z: " << bboxMax.z << "}\n";
    std::cout << "BboxMin: {x: " << bboxMin.x << ", y: " << bboxMin.y << ", z: " << bboxMin.z << "}\n";
    #endif
    for (int i = 0; i < modelDataVertices.size(); ++i)
    {
        glm::vec3 vertex = modelDataVertices[i];
        bboxMin = glm::min(vertex, bboxMin);
        bboxMax = glm::max(vertex, bboxMax);
    }
    centroid = (bboxMin + bboxMax) * 0.5f;
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

    glGenBuffers(1, &VBOTangents);
    glBindBuffer(GL_ARRAY_BUFFER, VBOTangents);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * modelDataTangents.size(), modelDataTangents.data(), GL_STATIC_DRAW);

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

    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, VBOTangents);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
}

void BuildBVH()
{
    nanort::BVHBuildOptions<float> build_options;  // Use default option
    build_options.cache_bbox = false;

    #ifdef OPTIMIZE
    #else
    printf("  BVH build option:\n");
    printf("    # of leaf primitives: %d\n", build_options.min_leaf_primitives);
    printf("    SAH binsize         : %d\n", build_options.bin_size);
    #endif
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

    #ifdef OPTIMIZE
    #else
    printf("  BVH statistics:\n");
    printf("    # of leaf   nodes: %d\n", stats.num_leaf_nodes);
    printf("    # of branch nodes: %d\n", stats.num_branch_nodes);
    printf("  Max tree depth     : %d\n", stats.max_tree_depth);
    #endif
    float bmin[3], bmax[3];
    accelDummy.BoundingBox(bmin, bmax);
    // bboxMax = glm::vec3(bmax[0], bmax[1], bmax[2]);
    // bboxMin = glm::vec3(bmin[0], bmin[1], bmax[2]);
    #ifdef OPTIMIZE
    #else
    printf("  Bmin               : %f, %f, %f\n", bmin[0], bmin[1], bmin[2]);
    printf("  Bmax               : %f, %f, %f\n", bmax[0], bmax[1], bmax[2]);
    #endif
    accel = accelDummy;
}

void recomputeCamera()
{
    glm::vec3 diff = calculateNearFarPlane() / radius;
    camPos   = centroid + diff * glm::vec3(0.0f, 0.0f, (far - near));
    camFront = glm::normalize(centroid - camPos);
	camUp    = glm::vec3(0.0f, 1.0f, 0.0f);
    std::cout << "Dif: " << glm::to_string(diff) << "\n";
    std::cout << "camPos: " << glm::to_string(camPos) << "\n";
    std::cout << "camFront: " << glm::to_string(camFront) << "\n";
    std::cout << "Radius: " << radius << "\n";
    std::cout << "Centroid: " << glm::to_string(centroid) << "\n";
    std::cout << "Focal Distance: " << focalDistance << "\n";
    std::cout << "Near: " << near << "\n";
    std::cout << "Far: " << far << "\n";
}

void reloadModel()
{
    BuildBVH();
    CreateBOs();
    model = glm::mat4(1.0f);
    std::cout << "Recomputed BBox Center: {x: " << centroid.x << ", y: " << centroid.y << ", z:" << centroid.z << "}\n";
}

float max(const glm::vec3& x)
{
    return std::max(std::max(x.x, x.y), x.z);
}

int main()
{
    /**
     * Start Window
     */
    int screenWidth = WIDTH, screenHeight = HEIGHT;
    
    SDL_Window* window;
    SDL_CreateWindowAndRenderer(screenWidth, screenHeight, 0, &window, nullptr);
    
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    if (!context)
    {
        #ifdef EXCEPTIONS
        throw std::runtime_error("Failed to create GL context!");
        #else
        std::cout << "Failed to create GL context!" << std::endl;
        #endif
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

    float time = 0.0f;
	// Mouse rotation globals.
	bool firstMouse = true;
	float lastX = ( float )( WIDTH ) * 0.5f, 
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
    Shader depthPrePass  = Shader("Shaders/DBuffer.vert",      "Shaders/DBuffer.frag");   
    geometryPass         = Shader("Shaders/GBuffer.vert",      "Shaders/GBuffer.frag");   
    Shader deferredPass  = Shader("Shaders/DeferredPass.vert", "Shaders/DeferredPass.frag");
    Shader hitPosition   = Shader("Shaders/HitPosition.vert",  "Shaders/HitPosition.frag");
    Shader decalsPass    = Shader("Shaders/Decals.vert",       "Shaders/Decals.frag");
    
    //ObjLoader("Assets/t-shirt-lp/source/Shirt.obj");

    #ifdef PILOT_SHIRT
    std::string fileName = "Assets/Pilot/shirt_1_lowPoly.gltf";
    //std::string fileName = "Assets/utahTeapot.gltf";
    #else
    std::string fileName = "Assets/t-shirt-lp/source/Shirt.gltf";
    #endif
    /**
     * Start Read GLTF
     */
    tinygltf::Model modelGLTF;
    if (!loadModel(modelGLTF, fileName.c_str()))
    {
        #ifdef EXCEPTIONS
        throw std::runtime_error("load GLTF Error!");
        #else
        std::cout << "Load GLTF Error!" << std::endl;
        #endif
    }

    loadGLTF(modelGLTF);
    /**
     * End Read GLFT
     */

    // Build the acceleration structure for ray tracing.
    BuildBVH();

    const float SPEED = 5.0f;
    //centroid = (bboxMin + bboxMax) * 0.5f;
    std::cout << "BBox Center: {x: " << centroid.x << ", y: " << centroid.y << ", z:" << centroid.z << "}\n";
	//camPos = centroid + glm::vec3(0.0f, 0.0f, 5.0f);//glm::vec3( 0.0f, -0.7f, 5.0f ); 

    geometryPass.use();

    #ifdef PILOT_SHIRT

    geometryPass.createTexture(&(material.normal), "Assets/Pilot/textures/T_DefaultMaterial_N_1k.jpg", "Normal", 1);
    // geometryPass.createTexture(&(material.roughness), "Assets/t-shirt-lp/textures/T_shirt_roughness.jpg", "Roughness", 2);
    // geometryPass.createTexture(&(material.metallic), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_metallic.png", "Metallic", 3);
    // geometryPass.createTexture(&(material.ao), "Assets/t-shirt-lp/textures/T_shirt_AO.jpg", "AmbientOcclussion", 4);
    geometryPass.createTexture(&(material.baseColor), "Assets/Pilot/textures/T_DefaultMaterial_B_1k.jpg", "BaseColor", 0);

    #else

    geometryPass.createTexture(&(material.normal), "Assets/t-shirt-lp/textures/T_shirt_normal.png", "Normal", 1);
    // geometryPass.createTexture(&(material.roughness), "Assets/t-shirt-lp/textures/T_shirt_roughness.jpg", "Roughness", 2);
    // geometryPass.createTexture(&(material.metallic), "Assets/Textures/rustediron1-alt2-Unreal-Engine/rustediron2_metallic.png", "Metallic", 3);
    // geometryPass.createTexture(&(material.ao), "Assets/t-shirt-lp/textures/T_shirt_AO.jpg", "AmbientOcclussion", 4);
    geometryPass.createTexture(&(material.baseColor), "Assets/t-shirt-lp/textures/T_shirt_albedo.jpg", "BaseColor", 0);

    #endif

    /** Start Depth Buffer **/
    unsigned int dBuffer;
    glGenFramebuffers(1, &dBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, dBuffer);

    depthPrePass.use();

    unsigned int rboDepth;

    // create and attach depth buffer (renderbuffer)
    glGenTextures(1, &rboDepth);
    glBindTexture(GL_TEXTURE_2D, rboDepth);
    #if 0
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, WIDTH/4, HEIGHT/4, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, rboDepth, 0);
    #else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, WIDTH/4, HEIGHT/4, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, rboDepth, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) 
    {
        std::cerr << "Framebuffer configuration failed" << std::endl;
    }
    #endif

    unsigned int attachments[1] = {GL_NONE};
    glDrawBuffers(1, attachments);

    /** End Depth Buffer **/

    /** Start Texture Space Buffer **/
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    geometryPass.use();
    #ifdef OPTIMIZE
    #else
    std::cout << "Albedo Width: "  << geometryPass.Width  << std::endl;
    std::cout << "Albedo Height: " << geometryPass.Height << std::endl;
    #endif

    glGenTextures(1, &renderAlbedo);
    glBindTexture(GL_TEXTURE_2D, renderAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, geometryPass.Width, geometryPass.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderAlbedo, 0);

    // Set the list of draw buffers.
    unsigned int drawBuffersFBO[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffersFBO); // "1" is the size of DrawBuffers

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    deferredPass.use();
    deferredPass.setInt("gNormal",    0);
    deferredPass.setInt("gAlbedo",    1);
    // deferredPass.setInt("gMetallic",  2);
    // deferredPass.setInt("gRoughness", 3);
    // deferredPass.setInt("gAO",        4);
    /** End Texture Space Buffer **/

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
    bool showBBox      = false;
    bool showProjector = false;
    bool showHitPoint  = false;
    bool normalMap     = false;
    
    int scale = 1;
    float blend = 0.5f;
	bool insideImGui = false;
    
	// Create the camera (eye).
	glm::mat4 view = glm::mat4(1.0f);
	model = glm::mat4(1.0f);

    float iTime = 0.0f;

    unsigned int decalTexture = -1;
    decalsPass.createTexture(&decalTexture, "Assets/Textures/Batman.jpg", "iChannel0", 1);
    decalsPass.setInt("iChannel1", 0);
    decalsPass.setInt("iDepth", 2);

    int flip = 0;

    int click = 0;

    int halfWidth = WIDTH/2, halfHeight = HEIGHT/2;

    glm::vec3 hitPos = glm::vec3(0.0, 0.0, 0.0);
    glm::vec3 hitNor = glm::vec3(1.0, 1.0, 1.0);

    unsigned int frame = 0,
                 counter = 0,
                 flipper = 0;

    float zoomSide = 0.5f,
          zoomTop  = zoomSide;

    glm::mat4 modelNoGuizmo = model;

    mousePositionX = screenWidth / 4;
    mousePositionY = screenHeight / 2;
    mousePositionX = mousePositionX + screenWidth / 4;

    glm::vec2 widthHeight = glm::vec2(screenWidth, screenHeight);

    recomputeCamera();

    glm::mat4 projection = glm::perspective(RADIANS_30, widthHeight.x / widthHeight.y, near, far);
    view = glm::lookAt(camPos, camPos + camFront, camUp);
    glm::mat4 viewPinned = view;

    rayTrace(mousePositionX, mousePositionY, widthHeight, modelDataVertices, 
             indexes, projection, view, model, true,
             /** Out **/ hitNor, hitPos, decalProjector);

    loop = [&]
    {
        //model = glm::mat4(1.0f);
        //model = glm::scale(model, glm::vec3(rNearFar));
        //model = glm::translate(model, -centroid);
        //modelNoGuizmo = model;
        
        if (decalImageBuffer.size() > 0)
        {
            //decalsPass.createTexture(&decalTexture, "Assets/Textures/WatchMen.jpeg", "iChannel0", 1);
            //decalsPass.createTextureFromFile(&decalTexture, decalImageBuffer, widthDecal, heightDecal, "iChannel0", 1);
            #ifdef OPTIMIZE
            #else
            std::cout << "Changing texture" << std::endl;
            #endif
            flip = 1;
            glGenTextures(1, &decalTexture);
            glBindTexture(GL_TEXTURE_2D, decalTexture);

            // In an ideal world this should be exposed as input params to the function.
            // Texture wrapping params.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            // Texture filtering params.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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

        widthHeight = glm::vec2(screenWidth, screenHeight);

        // No need to compute this every frame as the FOV stays always the same.
        glm::vec2 halfWidthHeight = widthHeight * glm::vec2(0.5, 0.5);
        float aspect = halfWidthHeight.x / halfWidthHeight.y;

        // calculateNearFarPlane(bboxMax, centroid, /** Out **/ near, far);
        // differenceBboxMaxMin = bboxMax - bboxMin;
		projection               = glm::perspective(RADIANS_30, widthHeight.x / widthHeight.y, near, far);
        glm::mat4 projectionHalf = glm::perspective(RADIANS_30, halfWidthHeight.x / widthHeight.y, near, far);                         
        glm::mat4 projectionSide //= glm::ortho(bboxMin.x, bboxMax.x, bboxMin.y, bboxMax.y, bboxMin.z, bboxMax.z);
                                 = glm::ortho(-aspect, aspect, -1.0f, 1.0f, near, far);
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
                #ifdef OPTIMIZE
                #else
				std::cout << "Mouse Button 1 (left) is pressed.\n";
                #endif
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
                rayTrace(mousePositionX, mousePositionY, widthHeight, modelDataVertices, 
                         indexes, projection, view, model, false,
                         /** Out **/ hitNor, hitPos, decalProjector);
            }

		}

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        ImGui::Begin("Graphical User Interface");   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        std::string printTime = std::to_string(deltaTime * 1000.0f) + " ms.\n";
		ImGui::TextUnformatted(printTime.c_str());
        if (ImGui::Button("Enable Normal Map"))
        {
            normalMap = !normalMap;
        }
        ImGui::SliderInt("Texture Coordinates Scale", &scale, 1, 10);
        ImGui::SliderFloat("Blend Factor", &blend, 0.0f, 1.0f);
        ImGui::SliderFloat("Projector Size", &projectorSize, 0.1f, 1.0f);
        ImGui::SliderFloat("Projector Orientation", &projectorRotation, 0.0f, 360.0f); 
        if (ImGui::Button("Show Bounding Box"))
        {
            showBBox = !showBBox;
        }
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
        /** Start Depth Pre-Pass **/
        glEnable(GL_DEPTH_TEST);
        glDepthMask(true);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, dBuffer); 

        depthPrePass.use();
        glBindVertexArray(VAO); 
        depthPrePass.setMat4("decalProjector", decalProjector);
        glViewport(0, 0, screenWidth/4, screenHeight/4);

        glClear(GL_DEPTH_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        /** End Depth Pre-Pass **/

        float flipFloat = (float) flip; 
        float flipAlbedoFloat = (float) flipAlbedo;


        /** Start Decal Pass **/
        glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        decalsPass.use();
        decalsPass.setMat4("model", model);
        decalsPass.setMat4("projection", projection);
        decalsPass.setMat4("view", view);
        decalsPass.setVec2("iResolution", widthHeight);
        decalsPass.setInt("iScale", scale);
        decalsPass.setFloat("iFlip", flipFloat);
        decalsPass.setFloat("iBlend", blend);
        decalsPass.setMat4("decalProjector", decalProjector);
        decalsPass.setFloat("iFlipAlbedo", flipAlbedoFloat);

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
        /** End Decal Pass **/

        /** Start Light Pass **/
        //glScissor(0, 0, screenWidth/2, screenHeight);
        int enableNormals = normalMap ? 1 : 0;
        glViewport(0, 0, screenWidth/2, screenHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(VAO);
        deferredPass.use();
        deferredPass.setMat4("model", model);
        deferredPass.setMat4("projection", projectionHalf);
        deferredPass.setMat4("view", view);
        deferredPass.setFloat("iFlipAlbedo", flipAlbedoFloat);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderAlbedo);
        // glActiveTexture(GL_TEXTURE2);
        // glBindTexture(GL_TEXTURE_2D, material.metallic);
        // glActiveTexture(GL_TEXTURE3);
        // glBindTexture(GL_TEXTURE_2D, material.ao);

        deferredPass.setVec3("viewPos", camPos);
        deferredPass.setFloat("iTime", iTime);
        deferredPass.setInt("iFlipper", flipper);
        deferredPass.setInt("iNormals", enableNormals);
        deferredPass.setFloat("iTime", iTime);
        // finally render quad
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        
        /** End Light Pass **/
        
        if (showBBox)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glEnable(GL_DEPTH_TEST);

            hitPosition.use();
            hitPosition.setMat4("model",      model);
            hitPosition.setMat4("projection", projectionHalf);
            hitPosition.setMat4("view",       view);
            renderLineCube(bboxMin, bboxMax);
        }

        if (showHitPoint)
        {
            glm::mat4 hitPositionModel = model;
            hitPositionModel = glm::translate(hitPositionModel, hitPos);
            hitPositionModel = glm::scale(hitPositionModel, glm::vec3(0.01f));

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
        modelSide = glm::rotate(modelSide, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        //modelSide = glm::translate(modelSide, glm::vec3(0.0f, -0.5f, 0.0f) * differenceBboxMaxMin);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(VAO);
        deferredPass.use();
        deferredPass.setMat4("model", modelSide);
        deferredPass.setMat4("projection", projectionSide);
        deferredPass.setMat4("view", viewPinned);/*glm::mat4(-0.113205, -0.187880, 0.975646, 0.000000, 
                                               0.000000, 0.981959, 0.189095, 0.000000, 
                                               -0.993572, 0.021407, -0.111162, 0.000000, 
                                               -0.064962, 0.388481, -4.221102, 1.000000));*/
        deferredPass.setFloat("iFlipAlbedo", flipAlbedoFloat);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderAlbedo);
        // glActiveTexture(GL_TEXTURE2);
        // glBindTexture(GL_TEXTURE_2D, material.metallic);
        // glActiveTexture(GL_TEXTURE3);
        // glBindTexture(GL_TEXTURE_2D, material.ao);

        deferredPass.setVec3("viewPos", camPos);
        deferredPass.setFloat("iTime", iTime);
        deferredPass.setInt("iFlipper", flipper);
        deferredPass.setInt("iNormals", 0);//enableNormals);
        deferredPass.setFloat("iTime", iTime);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        // Front View.
        glViewport(screenWidth/2, screenHeight/2, screenWidth/2, screenHeight/2);

        // Scale for the side view.
        glm::mat4 modelTop = glm::scale(modelNoGuizmo, glm::vec3(zoomTop, zoomTop, zoomTop));
        //modelTop = glm::translate(modelTop, glm::vec3(0.0f, -0.5f, 0.5f) * differenceBboxMaxMin);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(VAO);
        deferredPass.use();
        deferredPass.setMat4("model", modelTop);
        deferredPass.setMat4("projection", projectionSide);
        deferredPass.setMat4("view", viewPinned);/*glm::mat4(1.0, 0.0,  0.0, 0.0, 
                                               0.0, 1.0,  0.0, 0.0, 
                                               0.0, 0.0,  1.0, 0.0, 
                                               0.0, 0.0, -1.0, 1.0));*/
        deferredPass.setFloat("iFlipAlbedo", flipAlbedoFloat);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material.normal);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderAlbedo);
        // glActiveTexture(GL_TEXTURE2);
        // glBindTexture(GL_TEXTURE_2D, material.metallic);
        // glActiveTexture(GL_TEXTURE3);
        // glBindTexture(GL_TEXTURE_2D, material.ao);

        deferredPass.setVec3("viewPos", camPos);
        deferredPass.setFloat("iTime", iTime);
        deferredPass.setInt("iFlipper", flipper);
        deferredPass.setInt("iNormals", 0);//enableNormals);
        deferredPass.setFloat("iTime", iTime);

        glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_INT, 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

        SDL_PumpEvents();  // make sure we have the lascale mouse state.

        SDL_GL_GetDrawableSize(window, &screenWidth, &screenHeight);

        iTime += 1.0f / 100.0f;
        frame++;
        counter++;
        changeDecal = 0u;
    };

    emscripten_set_main_loop(main_loop, 0, true);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    modelDataVertices.clear();
    modelDataNormals.clear();
    modelDataTextureCoordinates.clear();
    modelDataTangents.clear();
    indexes.clear();
    //delete[] mesh.Vertices;
    //delete[] mesh.Normals;
    //delete[] mesh.Faces;
    //delete newVertices;
    //delete newNormals;
    //delete newTextureCoords;

    return EXIT_SUCCESS;
}
