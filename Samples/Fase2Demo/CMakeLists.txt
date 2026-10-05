add_executable(KizuriFase2Demo src/main.cpp)
target_link_libraries(KizuriFase2Demo PRIVATE KizuriCore KizuriRender)
add_custom_command(TARGET KizuriFase2Demo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:KizuriFase2Demo>/Shaders COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Samples/Assets $<TARGET_FILE_DIR:KizuriFase2Demo>/Assets)
