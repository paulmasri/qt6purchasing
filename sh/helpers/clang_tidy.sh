#!/usr/bin/env bash
set -e

BUILD_FOLDER="${PWD}/build"
MATCHER="${PWD}/src/.*"
COMPILE_COMMANDS="${BUILD_FOLDER}/compile_commands.json"

if [[ ! -d $BUILD_FOLDER ]]; then
    echo "Error: $BUILD_FOLDER is not a directory"
    exit 1
fi

if [[ ! -f $COMPILE_COMMANDS ]]; then
    echo "Error: $COMPILE_COMMANDS not found. Did you run cmake configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON?"
    exit 1
fi

# Restrict processed translation units to project sources under src/, so Qt's
# generated files in build/ (moc, rcc, qmltyperegistrations) are not linted.
run-clang-tidy \
    -p "${BUILD_FOLDER}/" \
    -header-filter "^${MATCHER}$" \
    -source-filter "^${MATCHER}$" \
    -use-color \
    "$@"

echo "=== Pass ==="
