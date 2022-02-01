set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

if (${PROJECT_NAME}_WANT_TESTS)
    add_subdirectory(googletest)
endif ()

option(CXXOPTS_BUILD_TESTS "Set to ON to build cxxopts tests" OFF)
add_subdirectory(cxxopts)
