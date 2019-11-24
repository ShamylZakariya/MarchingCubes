#!/bin/sh
find ./src \( -iname "*.h" -or -iname "*.hpp" -or -iname "*.cpp" \) | xargs clang-format -i -style=file
