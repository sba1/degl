CC=gcc
CXX=g++
CPPFLAGS=-g -I/usr/lib/llvm-3.5/include -Wall
CXXFLAGS=-std=c++11
LDLIBS=-g -L/usr/lib/llvm-3.5/lib -lclang

all: tests

degl: degl.cpp core.cpp core.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)  $< $(LDLIBS) -o $@

tests: core_test
	./core_test

core_test: core_test.cpp core.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS)  $< $(LDLIBS) -o $@

clean:
	rm -Rf core_test
