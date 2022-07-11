setup-release:
	export CC=clang
	export CXX=clang++
	meson setup --wipe --buildtype release --layout flat builddir

setup-debug:
	export CC=clang
	export CXX=clang++
	meson setup --wipe --buildtype debug --layout flat builddir

build:
	ninja -C builddir

run-hello: build
	./builddir/meson-out/hello_mc

run-terrain: build
	./builddir/meson-out/terrain
