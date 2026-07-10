CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

HEADERS = separate.h settling.h substance.h phase_table.h levigate.h fire.h magnetic.h smelt.h forge.h

test_separation: test_separation.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ test_separation.cpp

pan_game: pan.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ pan.cpp

ratchet: ratchet.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ ratchet.cpp

.PHONY: test pan ratchet-run clean
test: test_separation
	./test_separation

# Play the pan. Same physics as the tests, nothing added but legibility.
pan: pan_game
	./pan_game

# Walk Era 0 into Era 1 and watch the ratchet fail to turn. Not a test: a
# measurement, and it is allowed to come out badly. It did.
ratchet-run: ratchet
	./ratchet

clean:
	rm -f test_separation pan_game ratchet
