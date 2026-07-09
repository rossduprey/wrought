CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

test_separation: test_separation.cpp separate.h substance.h phase_table.h
	$(CXX) $(CXXFLAGS) -o $@ test_separation.cpp

.PHONY: test clean
test: test_separation
	./test_separation

clean:
	rm -f test_separation
