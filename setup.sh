#!/bin/sh
export CC=clang
export CXX=clang++
meson setup --buildtype debug --layout flat build
meson setup --buildtype release --layout flat build_release