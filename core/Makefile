CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

HEADERS = separate.h settling.h substance.h phase_table.h levigate.h fire.h magnetic.h smelt.h forge.h geology.h fuel.h firekit.h char.h knap.h haft.h split.h season.h dress.h

test_separation: test_separation.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ test_separation.cpp

pan_game: pan.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ pan.cpp

dig_game: dig.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ dig.cpp

breaker_game: breaker.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ breaker.cpp

furnace_game: furnace.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ furnace.cpp

carry_game: carry.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ carry.cpp

char_game: char.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ char.cpp

gather_game: gather.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ gather.cpp

firekit_game: firekit.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ firekit.cpp

knap_game: knap.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ knap.cpp

haft_game: haft.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ haft.cpp

ratchet: ratchet.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ ratchet.cpp

.PHONY: test pan dig breaker furnace carry char gather firekit knap haft ratchet-run clean
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

# Fire the ore into metal. What the breaker panned is still rock, mixing oxide and
# sulfide copper. The draft decides the fire: reduce to pour the oxide, but the
# sulfide must be roasted in open air first or it goes to slag. The two-fires
# finding, made a gesture -- and the recovery ceiling was set at the breaker.
furnace: furnace_game
	./furnace_game

# Haul the heap. The dig fills faster than hands empty, and the rock is one grade
# because nothing is freed at the hole -- so every kilogram left is ore left. The
# cart is the rung that unburies you, and distance is what makes it worth building.
carry: carry_game
	./carry_game

# Burn a pit of wood down to charcoal -- the fuel the furnace eats and never told
# you it was spending. The pit needs air to catch and come up hot, but the same fire
# left breathing burns the charcoal back to ash, so you must smother it once it is
# going: light, heat, choke, hold. The seal-is-the-gate finding, made a gesture.
char: char_game
	./char_game

# Knap a stone into an edge -- the first tool the hands can make, and the rung below
# every pick a dig assumes. The tough cobble gather let you pocket is only the hammer;
# the stone that takes an edge is a brittle flint you must hunt for in the streambed,
# and even a clean flake tops out at the stone's floor -- keen enough to cut a haft,
# never a razor, the wall only metal gets under. Fracture-not-hardness and the edge's
# irreducible floor, made a gesture: you strike, and the stone teaches you.
knap: knap_game
	./knap_game

# Haft the knapped edge to a stick and make the first real tool -- the one that fells
# the tree the whole staircase leans on. You do not sharpen the floored edge; you put
# it on a lever, where the same edge does what your fist never could. Work the joint
# until it seats (rush it and the head flies off, the edge delivered to nothing), set
# it as an axe, and swing: the tree yields to the FORCE the haft puts behind the edge,
# not to sharpness. Bring it down and the timber you could not reach is yours -- the
# bootstrap turns. The haft-is-force and joint-is-the-ceiling findings, made a gesture.
haft: haft_game
	./haft_game

# Gather the wood by hand -- and meet the wall the whole staircase leans on. You rake
# tinder and pull dead sticks, but the timber, the bulk of every trunk and the only
# stock a haft is cut from, stays standing: felling it wants an axe that wants a stone
# edge. Hands are tool #1, the worst point on the curve. The tool-bootstrap, made a
# gesture -- the forest full of wood your hands reach almost none of.
gather: gather_game
	./gather_game

# Make the fire. gather won you tinder and sticks; char lit its pile with a brand that
# came from nowhere. This is where the brand comes from: carve the smolder set (the
# first tool a person makes -- dry dead wood, a green spindle never smokes), build the
# ladder bottom-up, and STRIKE. No spark, no fire; tinder alone only flares; and the
# top rung, the smelt-grade timber fire, you cannot even lay -- it waits on the axe.
# The spark-is-a-made-tool and the ignition-ladder findings, made a gesture.
firekit: firekit_game
	./firekit_game

# Walk Era 0 into Era 1 and watch the ratchet fail to turn. Not a test: a
# measurement, and it is allowed to come out badly. It did.
ratchet-run: ratchet
	./ratchet

clean:
	rm -f test_separation pan_game dig_game breaker_game furnace_game carry_game char_game gather_game firekit_game knap_game haft_game ratchet
