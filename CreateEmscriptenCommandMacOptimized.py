from pathlib import Path
from applescript import tell

withAssertions = False
renderer = False
optimize = True

appPath = "/Users/felipe/Documents/Yukan/ShirtGenerator"
imguiPath = "/Users/felipe/Documents/Graphics/imgui/imgui/"
imguiBackendPath = imguiPath + "backends/"
brewPath = "/usr/local/include"
stbPath = "/Users/felipe/Documents/Graphics/stb/stb/"
tinyObjLoaderPath = "/Users/felipe/Documents/Graphics/tinyobjloader/tinyobjloader/"
tinyGLTFPath = "/Users/felipe/Documents/Graphics/tinygltf/"
currentPath = str(Path().absolute())
shaderPath = currentPath + "/Shaders@/Shaders"
assetsPath = currentPath + "/Assets@/Assets"
webGLVersion = "-s MIN_WEBGL_VERSION=2 "
webGLVersion += "-s MAX_WEBGL_VERSION=2 "
gladPath = "/Users/felipe/Documents/Graphics/Libraries/"
imGuizmoPath = "/Users/felipe/Documents/GitHub/ImGuizmo/ImGuizmo/"
nanoRTPath = "/Users/felipe/Documents/Graphics/nanort/ "
rendererClass = ""
if optimize:
    optimizationLevel = "-O2 "
else:
    optimizationLevel = ""
if renderer:
    rendererClass = "Renderer.cpp "
assertions = ""
if withAssertions:
    assertions = "-sASSERTIONS=2 "

emscriptenCommand = "emcc " + \
                    rendererClass + \
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
                    "-I" + brewPath + " " + \
                    "-I" + tinyObjLoaderPath + " " + \
                    "-I" + tinyGLTFPath + " " + \
                    "-I" + nanoRTPath + \
                    "-v --embed-file " + shaderPath + " " + \
                    "-v --preload-file " + assetsPath + " " + \
                    "-s FULL_ES3=0 " + \
                    "-s WASM=1 " + \
                    "-s USE_SDL=2 " + \
                    "-s GL_DEBUG=0 " + \
                    "-s FORCE_FILESYSTEM=1 " + \
                    webGLVersion + \
                    "-s ALLOW_MEMORY_GROWTH=1 " + \
                    "-s EXPORTED_RUNTIME_METHODS=ccall,cwrap " + \
                    optimizationLevel + \
                    assertions + \
                    "-o index.js"
print(emscriptenCommand)

yourCommand = 'cd ' + appPath + "; " + emscriptenCommand

tell.app( 'Terminal', 'do script "' + yourCommand + '"')
