cmake_minimum_required(VERSION 3.16)

project(
    psqlxx
    VERSION 0.0.1
    LANGUAGES C CXX)

find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --long --abbrev=7
    RESULT_VARIABLE result
    OUTPUT_VARIABLE GIT_COMMIT_DESCRIPTION
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if (result)
    message(WARNING "Failed to get Git description: ${result}")
endif ()

include_directories("${PROJECT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(utils)

config_cxx_compiler_and_linker()

# ######################################################################################
# CMake Standard Options
option(BUILD_SHARED_LIBS "Build libraries as .dll's/.so's." ON)
mark_as_advanced(CLEAR BUILD_SHARED_LIBS)

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)

# ######################################################################################
# Project Options
option(${PROJECT_NAME}_WARNINGS_AS_ERRORS "Treat all warnings as build errors." ON)

option(${PROJECT_NAME}_WANT_TESTS "Build all of the project's own tests." ON)
option(${PROJECT_NAME}_WANT_AUTO_TESTS
       "Automatically run tests that have been changed." ON)

if (${PROJECT_NAME}_WANT_TESTS)
    enable_testing()
    include(CTest)
    mark_as_advanced(BUILD_TESTING)

    include(GoogleTest)

    # cmake-lint: disable=C0103

    # Prevent GoogleTest from overriding our compiler/linker options when building with
    # Visual Studio
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE)

    include(test_defines)
endif ()

option(${PROJECT_NAME}_WANT_INSTALLER "Build the project's own installer." OFF)

if (${PROJECT_NAME}_WANT_INSTALLER)
    include(GNUInstallDirs)
    set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME})

    file(RELATIVE_PATH relative_dir ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}
         ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_INSTALL_RPATH $ORIGIN $ORIGIN/${relative_dir})

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT ${PROJECT_NAME}_Runtime)
    include(InstallRequiredSystemLibraries)
endif ()

# ######################################################################################
# Dependencies
add_subdirectory(3rdParty)

# ######################################################################################
# Main Targets
add_subdirectory(${PROJECT_NAME})

if (${PROJECT_NAME}_WANT_TESTS)
    add_subdirectory(tests)
    add_subdirectory(scripts)
endif ()

# ######################################################################################
# Packaging: Keep this block at the end
if (${PROJECT_NAME}_WANT_INSTALLER)
    if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_subdirectory(packaging)
    endif ()
endif ()
