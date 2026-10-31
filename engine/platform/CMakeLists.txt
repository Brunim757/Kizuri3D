add_library(caligo_platform STATIC
  src/File.cpp
  src/Input.cpp
  src/System.cpp
  src/Window.cpp
)

target_include_directories(caligo_platform PUBLIC include)

target_link_libraries(caligo_platform
  PUBLIC caligo_core
  PRIVATE user32 gdi32
)

caligo_configure_target(caligo_platform)
