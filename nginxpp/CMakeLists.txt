set(COMPILER_WARNING_OPTIONS -Wall -Wextra -pedantic-errors)
if (${PROJECT_NAME}_WARNINGS_AS_ERRORS)
    list(APPEND COMPILER_WARNING_OPTIONS -Werror)
endif ()

configure_file(version.cpp.in version.cpp @ONLY)
add_library(${PROJECT_NAME}_version STATIC ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
                                           version.hpp)
add_library(${PROJECT_NAME}::version ALIAS ${PROJECT_NAME}_version)
set_target_properties(${PROJECT_NAME}_version PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(${PROJECT_NAME}_version PRIVATE ${COMPILER_WARNING_OPTIONS})

add_library(
    ${PROJECT_NAME}_${PROJECT_NAME}
    args.cpp
    args.hpp
    exception.hpp
    message.cpp
    message.hpp
    server.cpp
    server.hpp
    string_utils.hpp)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME}_${PROJECT_NAME})
target_link_libraries(
    ${PROJECT_NAME}_${PROJECT_NAME}
    PRIVATE ${PROJECT_NAME}::version
    PUBLIC cxxopts Microsoft.GSL::GSL Threads::Threads)
target_compile_options(${PROJECT_NAME}_${PROJECT_NAME}
                       PUBLIC ${COMPILER_WARNING_OPTIONS})

add_executable(${PROJECT_NAME}_main main.cpp)
add_executable(${PROJECT_NAME}::main ALIAS ${PROJECT_NAME}_main)
target_link_libraries(${PROJECT_NAME}_main PRIVATE ${PROJECT_NAME}::${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}_main PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

if (${PROJECT_NAME}_WANT_TESTS)
    add_test(NAME ${PROJECT_NAME}.main.can_display_help COMMAND ${PROJECT_NAME}_main
                                                                --help)
    add_test(NAME ${PROJECT_NAME}.main.can_display_version COMMAND ${PROJECT_NAME}_main
                                                                   --version)
    enable_auto_test_command(${PROJECT_NAME}_main ^${PROJECT_NAME}.main)
endif ()

discover_gtest_for(server ${PROJECT_NAME}::${PROJECT_NAME})
discover_gtest_for(message ${PROJECT_NAME}::${PROJECT_NAME})

if (${PROJECT_NAME}_WANT_INSTALLER)
    install(
        TARGETS ${PROJECT_NAME}_main
        EXPORT ${PROJECT_NAME}_apps
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT ${PROJECT_NAME}_apps)
endif ()
