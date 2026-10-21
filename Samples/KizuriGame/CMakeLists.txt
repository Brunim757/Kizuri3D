add_executable(KizuriGame src/main.cpp)
target_link_libraries(KizuriGame PRIVATE KizuriCore KizuriRender KizuriPhysics KizuriAudio KizuriAnimation KizuriScripting KizuriScene KizuriAssets KizuriGame)
add_custom_command(TARGET KizuriGame POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Shaders $<TARGET_FILE_DIR:KizuriGame>/Shaders COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Samples/Assets $<TARGET_FILE_DIR:KizuriGame>/Assets)
