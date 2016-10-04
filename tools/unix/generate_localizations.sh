#!/bin/bash
set -e -u -x

echo "
******
    You can specify the name of string files for android and ios, in which case the resources will be generated for individual platforms. Otherwise the default strings.txt will be used.

    tools/unix/generate_localizations.sh android_strings.txt ios_strings.txt

******"

OMIM_PATH="$(dirname "$0")/../.."
TWINE="$OMIM_PATH/tools/twine/twine"

ANDROID_SOURCE_FILE=strings.txt
IOS_SOURCE_FILE=strings.txt

if [ $# == 2 ]; then
    ANDROID_SOURCE_FILE=$1
    IOS_SOURCE_FILE=$2
fi

# TODO: Add "--untagged --tags android" when tags are properly set.
# TODO: Add validate-strings-file call to check for duplicates (and avoid Android build errors) when tags are properly set.
$TWINE --format android generate-all-string-files "$OMIM_PATH/$ANDROID_SOURCE_FILE" "$OMIM_PATH/android/res/"
$TWINE --format apple generate-all-string-files "$OMIM_PATH/$IOS_SOURCE_FILE" "$OMIM_PATH/iphone/Maps/"
$TWINE --format apple --file-name InfoPlist.strings generate-all-string-files "$OMIM_PATH/iphone/plist.txt" "$OMIM_PATH/iphone/Maps/"
$TWINE --format jquery generate-all-string-files "$OMIM_PATH/data/cuisines.txt" "$OMIM_PATH/data/cuisine-strings/"
$TWINE --format jquery generate-all-string-files "$OMIM_PATH/data/countries_names.txt" "$OMIM_PATH/data/countries-strings/"
#$TWINE --format tizen generate-all-string-files "$OMIM_PATH/strings.txt" "$OMIM_PATH/tizen/MapsWithMe/res/" --tags tizen
