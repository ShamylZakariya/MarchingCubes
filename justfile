setup-release:
	export CC=clang
	export CXX=clang++
	meson setup --buildtype release build

setup:
	export CC=clang
	export CXX=clang++
	meson setup --buildtype debug build

build-release: setup-release
	meson compile -C build

build: setup
	meson compile -C build


run-hello: build
	./build/demos/hello_mc/hello_mc

run: build
	./build/demos/terrain/terrain
