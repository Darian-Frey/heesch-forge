# heesch-forge Linux build of upstream isohedral/heesch-sat.
# See ../UPSTREAM.md for the original macOS Makefile and the rationale
# for each change below (M0.3).

INCLUDES = -I/usr/local/include/cryptominisat5 -I/usr/local/include \
	$(shell pkg-config --cflags cairo)

CXX = g++
# OPT = -g
OPT = -O3 -DNDEBUG
# Upstream Makefile claimed -std=c++17, but tileio.h uses templated lambdas
# (C++20). Bumped here; recorded in ../UPSTREAM.md.
CXXFLAGS = -std=c++20 -Wall -MMD $(INCLUDES) $(OPT)
LIBS = -L/usr/local/lib -Wl,-rpath,/usr/local/lib -lcryptominisat5
VIZ_LIBS = $(shell pkg-config --libs cairo)

# isohedral.o is required by sat and surrounds (via heesch.h's
# IsohedralChecker). Upstream Makefile omits it; recorded in ../UPSTREAM.md.
OBJECTS = sat.o viz.o surrounds.o gen.o report.o isohedral.o
DEPENDS = ${OBJECTS:.o=.d}

# M1.1: a CaDiCaL-backed sat binary (sat-cadical) is built from the same
# sources with -DHEESCH_BACKEND_CADICAL. The `sat-cadical.o` rule below
# compiles sat.cpp with the macro; the link line adds -lcadical. Both
# binaries still link libcryptominisat5 because CMSat::Lit / CMSat::lbool
# remain the project-wide literal types.
CADICAL_OBJECTS = sat-cadical.o isohedral.o

all: sat viz gen surrounds report sat-cadical

# Link order matters with GNU ld: object files before libraries.
sat: sat.o isohedral.o
	$(CXX) -o sat sat.o isohedral.o $(LIBS)

sat-cadical.o: sat.cpp
	$(CXX) $(CXXFLAGS) -DHEESCH_BACKEND_CADICAL -c -o sat-cadical.o sat.cpp

sat-cadical: $(CADICAL_OBJECTS)
	$(CXX) -o sat-cadical $(CADICAL_OBJECTS) $(LIBS) -lcadical

viz: viz.o
	$(CXX) -o viz viz.o $(LIBS) $(VIZ_LIBS)

gen: gen.o
	$(CXX) -o gen gen.o $(LIBS)

surrounds: surrounds.o isohedral.o
	$(CXX) -o surrounds surrounds.o isohedral.o $(LIBS)

report: report.o
	$(CXX) -o report report.o $(LIBS)

# tile: tile.o
#	$(CXX) $(LIBS) -o tile tile.o

-include ${DEPENDS}

.PHONY: clean

clean:
	rm -f ${OBJECTS} ${DEPENDS} sat-cadical.o sat-cadical.d \
	      sat viz gen surrounds report sat-cadical
