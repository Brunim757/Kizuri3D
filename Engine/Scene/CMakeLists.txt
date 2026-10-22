add_library(KizuriScene STATIC src/Scene.cpp src/ComponentRegistry.cpp src/MultiSelection.cpp src/Log.cpp src/EditQueue.cpp src/Picking.cpp src/Reflection.cpp src/SceneSerializer.cpp src/Undo.cpp src/Autosave.cpp src/Notifications.cpp)
target_include_directories(KizuriScene PUBLIC include)
target_link_libraries(KizuriScene PUBLIC KizuriCore)
