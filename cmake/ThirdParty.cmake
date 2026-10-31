include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ENKITS_INSTALL OFF CACHE BOOL "" FORCE)

set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  DirectXMath
  GIT_REPOSITORY https://github.com/microsoft/DirectXMath.git
  GIT_TAG oct2024
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  enkiTS
  GIT_REPOSITORY https://github.com/dougbinks/enkiTS.git
  GIT_TAG v1.12
  GIT_SHALLOW TRUE
)

if(CALIGO_BUILD_TESTS)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(DirectXMath enkiTS doctest)
else()
  FetchContent_MakeAvailable(DirectXMath enkiTS)
endif()
