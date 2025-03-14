all: qregd

qregd: build/qregd
	cp build/qregd .

build/qregd: qregd.c myiio.c cmd.c cmd.h parse.c parse.h build/Makefile
	make -C build

build/Makefile: CMakeLists.txt
	mkdir -p build
	cd build; cmake ..
