CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

HEADERS = separate.h settling.h substance.h phase_table.h levigate.h fire.h magnetic.h smelt.h forge.h geology.h

test_separation: test_separation.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ test_separation.cpp

pan_game: pan.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ pan.cpp

dig_game: dig.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ dig.cpp

breaker_game: breaker.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ breaker.cpp

ratchet: ratchet.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ ratchet.cpp

.PHONY: test pan dig breaker ratchet-run clean
test: test_separation
	./test_separation

# Play the pan. Same physics as the tests, nothing added but legibility.
pan: pan_game
	./pan_game

# Work the ground. The geology field made playable: strike down through the
# layers, watch the mineral change with depth, carry off locked rock.
dig: dig_game
	./dig_game

# Break the rock. What the dig hands you comes up locked; the hammer frees it and
# the pan keeps it -- crush too little and the pan wins nothing, too much and it
# washes over the lip. The grade/recovery law, made a gesture.
breaker: breaker_game
	./breaker_game

# Walk Era 0 into Era 1 and watch the ratchet fail to turn. Not a test: a
# measurement, and it is allowed to come out badly. It did.
ratchet-run: ratchet
	./ratchet

clean:
	rm -f test_separation pan_game dig_game breaker_game ratchet
