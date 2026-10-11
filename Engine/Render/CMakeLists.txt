add_library(KizuriRender STATIC src/Render.cpp src/RHI.cpp src/RHI_Null.cpp src/RHI_D3D11.cpp src/Camera.cpp src/MeshLoader.cpp src/PBR.cpp src/DeferredRenderer.cpp src/DebugDraw.cpp src/Shadows.cpp src/RenderGraph.cpp)
target_include_directories(KizuriRender PUBLIC include)
target_include_directories(KizuriRender PRIVATE ${CMAKE_SOURCE_DIR}/Shaders/fsr)
target_link_libraries(KizuriRender PUBLIC kizuri_imgui kizuri_imguizmo kizuri_cgltf kizuri_stb d3d11 dxgi d3dcompiler)
