add_executable(caligo_tests
  main.cpp
  core/test_result.cpp
  core/test_log.cpp
  core/test_time.cpp
  core/test_job_system.cpp
  core/test_math.cpp
  platform/test_input.cpp
  platform/test_file.cpp
)

target_link_libraries(caligo_tests PRIVATE
  caligo_core
  caligo_platform
  doctest::doctest
)

caligo_configure_target(caligo_tests)

add_test(NAME caligo_tests COMMAND caligo_tests)
add_test(NAME caligo_headless COMMAND caligo --headless --frames 120)
set_tests_properties(caligo_headless PROPERTIES TIMEOUT 60)
