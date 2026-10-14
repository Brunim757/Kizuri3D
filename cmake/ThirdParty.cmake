include(FetchContent)
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_C_INTERFACE ON CACHE BOOL "" FORCE)
set(ENKITS_INSTALL OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATHASH OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  DirectXMath
  GIT_REPOSITORY https://github.com/microsoft/DirectXMath.git
  GIT_TAG oct2024
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  enkiTS
  GIT_REPOSITORY https://github.com/dougbinks/enkiTS.git
  GIT_TAG v1.12
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  zstd
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG v1.5.6
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake
)
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG v25.12.19
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(DirectXMath enkiTS zstd flatbuffers)
set(KIZURI_PACKAGES_DIR ${CMAKE_BINARY_DIR}/kizuri-packages)
file(MAKE_DIRECTORY ${KIZURI_PACKAGES_DIR})
file(WRITE ${KIZURI_PACKAGES_DIR}/directxmath-config.cmake "add_library(Microsoft::DirectXMath INTERFACE IMPORTED)\nset_target_properties(Microsoft::DirectXMath PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${directxmath_SOURCE_DIR}/Inc\")\n")
set(directxmath_DIR ${KIZURI_PACKAGES_DIR} CACHE PATH "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  DirectXTex
  GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
  GIT_TAG may2026
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(DirectXTex)
set(DRACO_TESTS OFF CACHE BOOL "" FORCE)
set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "" FORCE)
set(DRACO_WASM OFF CACHE BOOL "" FORCE)
set(DRACO_UNITY_PLUGIN OFF CACHE BOOL "" FORCE)
set(DRACO_MAYA_PLUGIN OFF CACHE BOOL "" FORCE)
set(DRACO_JS_GLUE OFF CACHE BOOL "" FORCE)
set(DRACO_ANIMATION_ENCODING OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_FUZZTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  draco
  GIT_REPOSITORY https://github.com/google/draco.git
  GIT_TAG 1.5.7
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  libwebp
  GIT_REPOSITORY https://github.com/webmproject/libwebp.git
  GIT_TAG v1.5.0
  GIT_SHALLOW TRUE
)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat)
FetchContent_MakeAvailable(draco libwebp)
add_library(kizuri_draco INTERFACE)
target_link_libraries(kizuri_draco INTERFACE draco)
target_include_directories(kizuri_draco INTERFACE ${draco_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
add_library(kizuri_webp INTERFACE)
target_link_libraries(kizuri_webp INTERFACE webpdecoder)
target_include_directories(kizuri_webp INTERFACE ${libwebp_SOURCE_DIR}/src)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9-docking
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  imguizmo
  GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG v1.15
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.25
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  Jolt
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v5.0.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(Jolt)
if(MSVC)
  target_compile_options(Jolt PRIVATE /WX-)
endif()
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
  FetchContent_Populate(imguizmo)
endif()
FetchContent_GetProperties(cgltf)
if(NOT cgltf_POPULATED)
  FetchContent_Populate(cgltf)
endif()
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
  FetchContent_Populate(stb)
endif()
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
  FetchContent_Populate(miniaudio)
endif()
add_library(kizuri_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(kizuri_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
add_library(kizuri_imguizmo STATIC
  ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)
target_include_directories(kizuri_imguizmo PUBLIC ${imguizmo_SOURCE_DIR}/src)
target_link_libraries(kizuri_imguizmo PUBLIC kizuri_imgui)
add_library(kizuri_cgltf STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf_impl.cpp
)
target_include_directories(kizuri_cgltf PUBLIC ${cgltf_SOURCE_DIR})
add_library(kizuri_mikktspace STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c
)
target_include_directories(kizuri_mikktspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace)
add_library(kizuri_meshopt STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt/simplifier.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt/indexcodec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt/vertexcodec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt/vertexfilter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt/vcacheoptimizer.cpp
)
target_include_directories(kizuri_meshopt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt)
add_library(kizuri_basisu STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/basisu/basisu_transcoder.cpp
)
target_include_directories(kizuri_basisu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/basisu)
target_compile_definitions(kizuri_basisu PRIVATE BASISD_SUPPORT_KTX2_ZSTD=0)
if(MSVC)
  target_compile_definitions(kizuri_basisu PRIVATE BASISU_NO_ITERATOR_DEBUG_LEVEL)
endif()
add_library(kizuri_xatlas STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xatlas/xatlas.cpp
)
target_include_directories(kizuri_xatlas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xatlas)
add_library(kizuri_stb STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb_impl.cpp
)
target_include_directories(kizuri_stb PUBLIC ${stb_SOURCE_DIR})
add_library(kizuri_miniaudio STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio_impl.cpp
)
target_include_directories(kizuri_miniaudio PUBLIC ${miniaudio_SOURCE_DIR})
