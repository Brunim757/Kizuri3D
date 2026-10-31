add_library(caligo_core STATIC
  src/Assert.cpp
  src/JobSystem.cpp
  src/Log.cpp
  src/Time.cpp
)

target_include_directories(caligo_core PUBLIC include)

target_link_libraries(caligo_core
  PUBLIC DirectXMath
  PRIVATE enkiTS
)

caligo_configure_target(caligo_core)
