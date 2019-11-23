#!/bin/sh
find ./src \( -iname "*.h" -or -iname "*.cpp" \) | xargs clang-format -i -style=file
