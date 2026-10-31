add_executable(caligo
  src/main.cpp
  src/Application.cpp
)

target_link_libraries(caligo PRIVATE caligo_platform)

target_compile_definitions(caligo PRIVATE
  CALIGO_VERSION="${PROJECT_VERSION}"
  CALIGO_CONFIGURATION="$<CONFIG>"
)

caligo_configure_target(caligo)
