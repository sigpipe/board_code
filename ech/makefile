all: ech

ech: build/ech 
	cp build/ech .
mon: build/mon 
	cp build/mon .

build/ech: ech.c myiio.c build/Makefile
	make -C build
build/mon: mon.c myiio.c build/Makefile
	make -C build


build/Makefile: CMakeLists.txt
	mkdir -p build
	cd build; cmake ..

