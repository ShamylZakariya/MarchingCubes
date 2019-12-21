#!/bin/sh
find ./src \( -iname "*.h" -or -iname "*.hpp" -or -iname "*.cpp" -or -iname "*.mm" \) | xargs clang-format -i -style=file
