CC=gcc
CXX=g++
CPPFLAGS=-I/usr/lib/llvm-3.5/include -Wall
CXXFLAGS=-std=c++11
LDLIBS=-L/usr/lib/llvm-3.5/lib -lclang

all: tests

tests: core_test
	./core_test

core_test: core_test.cpp core.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)  $< $(LDLIBS) -o $@

clean:
	rm -Rf core_test
