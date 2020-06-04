#!/bin/sh
export CC=clang
export CXX=clang++
meson setup --buildtype release --layout flat build_release