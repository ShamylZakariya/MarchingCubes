#!/bin/sh
find ./mc \( -iname "*.h" -or -iname "*.hpp" -or -iname "*.cpp" \) | xargs clang-format -i -style=file
find ./demos \( -iname "*.h" -or -iname "*.hpp" -or -iname "*.cpp" \) | xargs clang-format -i -style=file

