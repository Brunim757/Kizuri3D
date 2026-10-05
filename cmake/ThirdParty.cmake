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
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/cgltf_impl.cpp
)
target_include_directories(kizuri_cgltf PUBLIC ${cgltf_SOURCE_DIR})
add_library(kizuri_mikktspace STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace/mikktspace.c
)
target_include_directories(kizuri_mikktspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mikktspace)
add_library(kizuri_meshopt STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt/simplifier.cpp
)
target_include_directories(kizuri_meshopt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshopt)
add_library(kizuri_xatlas STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xatlas/xatlas.cpp
)
target_include_directories(kizuri_xatlas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xatlas)
add_library(kizuri_stb STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/stb_impl.cpp
)
target_include_directories(kizuri_stb PUBLIC ${stb_SOURCE_DIR})
add_library(kizuri_miniaudio STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/miniaudio_impl.cpp
)
target_include_directories(kizuri_miniaudio PUBLIC ${miniaudio_SOURCE_DIR})
