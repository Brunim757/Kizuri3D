add_library(KizuriGame STATIC
  src/GameLoop.cpp
  src/GameWorld.cpp
  src/GameApp.cpp
  src/GameMaterial.cpp
  src/HardwareSpec.cpp
)
target_include_directories(KizuriGame PUBLIC include)
target_link_libraries(KizuriGame PUBLIC KizuriCore KizuriRender KizuriPhysics KizuriAudio KizuriScene KizuriAssets d3d11 dxgi)
