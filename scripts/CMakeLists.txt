find_program(BASH_PROGRAM bash)

if (BASH_PROGRAM)
    add_test(NAME ${PROJECT_NAME}.format_tests
             COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/format_tests.sh)
endif (BASH_PROGRAM)
