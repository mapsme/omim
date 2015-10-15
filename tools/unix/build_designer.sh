#!/bin/bash

set -e -u

if [ $# == 0 ]
then
  echo "Usage: $0 DATA_VERSION [APP_VERSION]"
  echo "example of DATA_VERSION : 150902"
  echo "example of APP_VERSION  : 1.2.3"
  exit 1
fi

DESIGNER_DATA_VERSION=$1
APP_VERSION=UNKNOWN
[ $# -gt 1 ] && APP_VERSION=$2
DESIGNER_CODEBASE_SHA=$(git log -1 --format="%H")

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
cp "$DATA_PATH/city_rank.txt" "$BUILD_PATH/deploy/styles/clear/style-clear/city_rank.txt"

DMG_NAME=MAPS.ME.Designer.$APP_VERSION
hdiutil create -size 240m -volname $DMG_NAME -srcfolder "$BUILD_PATH/deploy" -ov -format UDZO "$BUILD_PATH/$DMG_NAME.dmg"
