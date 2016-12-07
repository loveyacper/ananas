all:
	g++ -Dthread_local=__thread -O2  -Wall -std=c++0x *.cc Buffer/*.cc Threads/*.cc Util/*.cc  File/*.cc -lpthread
clean:
	-rm -rf logdir* a.out
