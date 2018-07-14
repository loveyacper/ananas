all:
	mkdir -p build && cd build && cmake .. && make
install:
	cd build && make install
clean:
	cd build && make clean
