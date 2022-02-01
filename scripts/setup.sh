#!/bin/bash

#
# This script sets up a project to be used for the first time
#

set -ex

THIS_DIR=$(dirname "$0")
source "$THIS_DIR/utils.sh"

QuietRun pushd "$THIS_DIR"
PROJECT_ROOT_DIR=$(GetProjectRootDir)
QuietRun popd

QuietRun pushd "$PROJECT_ROOT_DIR"
PROJECT_NAME=$(basename "$PROJECT_ROOT_DIR")
echo "Setting up project \"$PROJECT_NAME\"."

./scripts/bootstrap.sh

rm build/ -rf
ctest --build-and-test . build \
    --build-generator "Unix Makefiles" \
    --build-options \
    -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
    -DBUILD_SHARED_LIBS:BOOL=ON \
    --test-command ctest -j 2

../config-sh/scripts/setup_ctags_for.sh
QuietRun popd
