from pathlib import Path
import os

withAssertions = False
optimize = True

graphicsPath = "C:\\SSS\\Graphics"
imguiPath = graphicsPath + "\\imgui\\"
imguiBackendPath = imguiPath + "backends\\"
glmPath = graphicsPath + "\\glm\\"
stbPath = graphicsPath + "\\stb\\"
tinyObjLoaderPath = graphicsPath + "\\tinyobjloader\\"
imGuizmoPath = graphicsPath + "\\ImGuizmo\\"
currentPath = str(Path().absolute())
shaderPath = currentPath + "/Shaders@/Shaders"
assetsPath = currentPath + "/Assets@/Assets"
webGLVersion = "-s MIN_WEBGL_VERSION=2 "
webGLVersion += "-s MAX_WEBGL_VERSION=2 "

if optimize:
    optimizationLevel = "-O2 "
else:
    optimizationLevel = ""
assertions = ""
if withAssertions:
    assertions = "-sASSERTIONS=2 "

emscriptenCommand = "emcc " + \
                    "-g " + \
                    imguiPath + "imgui.cpp " + \
                    imguiPath + "imgui_draw.cpp " + \
                    imguiPath + "imgui_tables.cpp " + \
                    imguiPath + "imgui_widgets.cpp " + \
                    imguiBackendPath + "imgui_impl_sdl.cpp " + \
                    imguiBackendPath + "imgui_impl_opengl3.cpp " + \
                    imGuizmoPath + "ImGuizmo.cpp " + \
                    "main.cpp " + \
                    "-I" + imguiPath + " " + \
                    "-I" + imguiBackendPath + " " + \
                    "-I" + imGuizmoPath + " " + \
                    "-I" + glmPath + " " + \
                    "-I" + stbPath + " " + \
                    "-I" + tinyObjLoaderPath + " " + \
                    "-v --embed-file " + shaderPath + " " + \
                    "-v --preload-file " + assetsPath + " " + \
                    "-s FULL_ES3=1 " + \
                    "-s WASM=1 " + \
                    "-s USE_SDL=2 " + \
                    "-s GL_DEBUG=1 " + \
                    webGLVersion + \
                    "-s ALLOW_MEMORY_GROWTH=1 " + \
                    optimizationLevel + \
                    assertions + \
                    "-o index.js"
print(emscriptenCommand)
os.system(emscriptenCommand + " & pause")
