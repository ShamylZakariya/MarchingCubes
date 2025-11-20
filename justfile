setup-release:
	export CC=clang
	export CXX=clang++
	meson setup --buildtype release build

setup:
	export CC=clang
	export CXX=clang++
	meson setup --buildtype debug build_dbg

build-release: setup-release
	meson compile -C build

build: setup
	meson compile -C build_dbg


run-hello: build
	./build_dbg/demos/hello_mc/hello_mc

run: build
	./build_dbg/demos/terrain/terrain
