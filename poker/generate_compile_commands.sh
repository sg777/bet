#!/bin/bash
# Generate compile_commands.json for language server support

EXTERNAL_DIR=$(cd ../external && pwd)
TARGET_DIR=$(cd ../external && cd $(gcc -dumpmachine) && pwd)
LIBWEBSOCKETS_HEADERS="$TARGET_DIR/libwebsockets-build/include"
LIBJSMN_HEADERS="$EXTERNAL_DIR/jsmn"
LIBINIPARSER_HEADERS="$EXTERNAL_DIR/iniparser-build/src"

echo "["
first=true
for src in src/*.c; do
    if [ "$first" = true ]; then
        first=false
    else
        echo ","
    fi
    cat << JSON
  {
    "directory": "$(pwd)",
    "command": "gcc -g -fPIC -std=c99 -Iinclude -I../libs/crypto -I$EXTERNAL_DIR/includes/curl -I$EXTERNAL_DIR/dlg/include -I$EXTERNAL_DIR/iniparser/src -I$EXTERNAL_DIR/nng/include -I$EXTERNAL_DIR/jsmn -I$LIBINIPARSER_HEADERS -I$LIBWEBSOCKETS_HEADERS -I$LIBJSMN_HEADERS -I../includes/curl -c $src",
    "file": "$(pwd)/$src"
  }JSON
done
echo ""
echo "]"
