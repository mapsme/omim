#!/bin/bash

set -e -u

DEFAULT_DESIGNER_DATA_VERSION=151113

# Prepare environment variables which specify data version, app version and latest sha
DESIGNER_DATA_VERSION=$DEFAULT_DESIGNER_DATA_VERSION
[ $# -gt 0 ] && DESIGNER_DATA_VERSION=$1
APP_VERSION=UNKNOWN
[ $# -gt 1 ] && APP_VERSION=$2
DESIGNER_CODEBASE_SHA=$(git log -1 --format="%H")

# If user has specified only one param '-' then just configure environment for the default data version
if [ $# -eq 1 ]; then
  if [[ "$1" == "-" ]]; then
    DESIGNER_DATA_VERSION=$DEFAULT_DESIGNER_DATA_VERSION
    APP_VERSION="-"
  fi
fi

echo "App version: $APP_VERSION"
echo "Data version: $DESIGNER_DATA_VERSION"
echo "Commit SHA: $DESIGNER_CODEBASE_SHA"

OMIM_PATH="$(dirname "$0")/../.."
DATA_PATH="$OMIM_PATH/data"
BUILD_PATH="$OMIM_PATH/out"
RELEASE_PATH="$BUILD_PATH/release"

# Print designer_version.h file
cat > "$OMIM_PATH/designer_version.h" <<DVER
#pragma once
#define DESIGNER_APP_VERSION "$APP_VERSION"
#define DESIGNER_CODEBASE_SHA "$DESIGNER_CODEBASE_SHA"
#define DESIGNER_DATA_VERSION "$DESIGNER_DATA_VERSION"
DVER

# Load designer's version of countries.txt and World and WorldCoasts mwms
for file in World.mwm WorldCoasts.mwm countries.txt; do
  rm "$DATA_PATH/$file" || true
  curl -o "$DATA_PATH/$file" "http://designer.mapswithme.com/mac/$DESIGNER_DATA_VERSION/$file"
done

# Substitute tools/autobuild/build.sh
rm "$OMIM_PATH/tools/autobuild/build.sh"
cp "$OMIM_PATH/tools/autobuild/build_designer.sh" "$OMIM_PATH/tools/autobuild/build.sh"

# If user has specified app version as '-' then just configure environment
if [[ "$APP_VERSION" == "-" ]]; then
  echo "MAPS.ME.Designer environment has been configured"
  exit 0
fi

rm -rf "$RELEASE_PATH"
(
  cd "$OMIM_PATH"
  ${QMAKE-qmake} omim.pro -r -spec macx-clang CONFIG+=release CONFIG+=x86_64 CONFIG+=map_designer
  make
)

# Prepare app package by copying Qt, Kothic and Skin Generator
macdeployqt "$RELEASE_PATH/skin_generator.app"
macdeployqt "$RELEASE_PATH/MAPS.ME.Designer.app"
MAC_RESOURCES="$RELEASE_PATH/MAPS.ME.Designer.app/Contents/Resources"
mkdir "$MAC_RESOURCES/skin_generator"
cp -r "$RELEASE_PATH/skin_generator.app" "$MAC_RESOURCES/skin_generator/skin_generator.app"
cp -r "$OMIM_PATH/tools/kothic" "$MAC_RESOURCES/kothic"
cp "$OMIM_PATH/protobuf/protobuf-2.6.1-py2.7.egg" "$MAC_RESOURCES/kothic"

# Build DMG image
rm -rf "$BUILD_PATH/deploy"
mkdir "$BUILD_PATH/deploy"
cp -r "$RELEASE_PATH/MAPS.ME.Designer.app" "$BUILD_PATH/deploy/MAPS.ME.Designer.app"
cp -r "$DATA_PATH/styles" "$BUILD_PATH/deploy/styles"

DMG_NAME=MAPS.ME.Designer.$APP_VERSION
hdiutil create -size 240m -volname $DMG_NAME -srcfolder "$BUILD_PATH/deploy" -ov -format UDZO "$BUILD_PATH/$DMG_NAME.dmg"
