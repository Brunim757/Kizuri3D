add_library(KizuriPhysics STATIC
  src/Physics.cpp
  src/PhysicsWorld.cpp
  src/JobSystemEnki.cpp
)
target_include_directories(KizuriPhysics PUBLIC include)
target_link_libraries(KizuriPhysics PUBLIC Jolt KizuriCore)
