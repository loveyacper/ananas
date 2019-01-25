all:
	mkdir -p build && cd build && cmake .. && make
doc:
	doxygen Doxyfile
	ln -s -f api_docs/index.html ananas.html
install:
	cd build && make install
clean:
	cd build && make clean
