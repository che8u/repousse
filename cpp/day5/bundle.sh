#!/bin/bash

# This script packages the compiled executable and resources into a macOS .app bundle.
# It can also be used to clean up the created bundle.
#
# Usage:
#   ./bundle.sh        - Creates the .app bundle.
#   ./bundle.sh clean  - Removes the .app bundle.

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration ---
APP_NAME="Fractal"
BUILD_DIR="build"
PLIST_FILE="Info.plist"
ICON_FILE="assets/app.icns"

# --- Paths ---
# The final .app bundle that will be created
BUNDLE_PATH="${BUILD_DIR}/${APP_NAME}.app"

# Paths to the directories inside the .app bundle
CONTENTS_PATH="${BUNDLE_PATH}/Contents"
MACOS_PATH="${CONTENTS_PATH}/MacOS"
RESOURCES_PATH="${CONTENTS_PATH}/Resources"

# Paths to the source files that will be copied
EXEC_PATH="${BUILD_DIR}/${APP_NAME}"
METAL_LIB_PATH="${BUILD_DIR}/fractal.metallib"


# Removes any existing .app bundle.
function clean_bundle() {
    echo "  -> Removing old bundle..."
    rm -rf "${BUNDLE_PATH}"
}

# Creates the .app bundle structure and copies all necessary files.
function create_bundle() {
    echo "  -> Creating directory structure..."
    mkdir -p "${MACOS_PATH}"
    mkdir -p "${RESOURCES_PATH}"

    echo "  -> Copying files..."
    cp "${EXEC_PATH}" "${MACOS_PATH}/"
    cp "${PLIST_FILE}" "${CONTENTS_PATH}/Info.plist"
    cp "${ICON_FILE}" "${RESOURCES_PATH}/app.icns"
    cp "${METAL_LIB_PATH}" "${RESOURCES_PATH}/fractal.metallib"
}


COMMAND=$1
if [[ "${COMMAND}" == "clean" ]]; then
    echo "Cleaning up bundle..."
    clean_bundle
    echo "Bundle cleaned."

elif [[ -z "${COMMAND}" ]]; then # Default action if no command is given
    echo "Creating .app bundle for ${APP_NAME}..."
    clean_bundle # Always start with a clean slate
    create_bundle
    echo ""
    echo "Successfully created ${BUNDLE_PATH}"

else
    echo "Error: Unknown command '${COMMAND}'"
    echo "Usage: $0 [clean]"
    exit 1
fi
