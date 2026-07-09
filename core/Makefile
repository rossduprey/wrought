CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

HEADERS = separate.h settling.h substance.h phase_table.h

test_separation: test_separation.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ test_separation.cpp

pan_game: pan.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ pan.cpp

.PHONY: test pan clean
test: test_separation
	./test_separation

# Play the pan. Same physics as the tests, nothing added but legibility.
pan: pan_game
	./pan_game

clean:
	rm -f test_separation pan_game
