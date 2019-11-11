
# Makefile for c++ project of Dash-Parser
#

CXX=clang++-8
LIBRARIES=-lz
SOURCES=main.cpp EncodeDetect.cpp 

all:
	$(CXX) $(SOURCES) $(LIBRARIES) -o test.native

run:
	make clean
	$(CXX) $(SOURCES) $(LIBRARIES) -o test.native
	./test.native

clean:
	rm -rf *.native
