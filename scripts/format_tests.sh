#!/bin/bash

oneTimeSetUp() {
    THIS_DIR=$(dirname "$0")
    source "$THIS_DIR/utils.sh"
}

testNoUnexpectedKeywordsOrder() {
    PROJECT_ROOT_DIR=$(GetProjectRootDir)
    QuietRun pushd "$PROJECT_ROOT_DIR"

    assertFalse "git grep 'constexpr inline' -- ':(exclude)*.sh'"
    assertFalse "git grep 'const inline' -- ':(exclude)*.sh'"
    assertFalse "git grep 'inline static' -- ':(exclude)*.sh'"

    QuietRun popd
}

source /usr/bin/shunit2
