all:
	mkdir -p build && cd build && cmake .. && make
clean:
	cd build && make clean
