add_executable(KizuriEditor src/main.cpp src/EditorApp.cpp src/EditorPanels.cpp src/FileDialog.cpp)
target_link_libraries(KizuriEditor PRIVATE KizuriCore KizuriRender KizuriPhysics KizuriAnimation KizuriAudio KizuriScripting KizuriScene KizuriAssets comdlg32 shell32)
add_custom_command(TARGET KizuriEditor POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:KizuriEditor>/Shaders COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Samples/Assets $<TARGET_FILE_DIR:KizuriEditor>/Assets)
