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

# M1.1 / M1.2a: extra backend-specific binaries built from the same sources
# with a -DHEESCH_BACKEND_* macro. Each variant gets its own .o so the
# CMSat default build is unaffected. All variants still link
# libcryptominisat5 because CMSat::Lit / CMSat::lbool stay as the project-
# wide literal types.
CADICAL_OBJECTS   = sat-cadical.o isohedral.o
KISSAT_OBJECTS    = sat-kissat.o isohedral.o
DUMP_OBJECTS      = sat-dump.o isohedral.o
WCNF_DUMP_OBJECTS = sat-wcnf-dump.o isohedral.o

all: sat viz gen surrounds report sat-cadical sat-kissat sat-dump sat-wcnf-dump

CORONA_DIR = ../../corona
CORONA_LIB = $(CORONA_DIR)/libcorona.a

# M2.6-followup-A: sat needs CoronaState::read for the -seed flag, so we
# pull in libcorona.a (which transitively needs libdlx.a too). The
# recursive make builds the corona library if it's stale or missing.
$(CORONA_LIB):
	$(MAKE) -C $(CORONA_DIR) libcorona.a

# Link order matters with GNU ld: object files before libraries.
sat: sat.o isohedral.o $(CORONA_LIB)
	$(CXX) -o sat sat.o isohedral.o $(CORONA_LIB) ../../dlx/libdlx.a $(LIBS)

sat-cadical.o: sat.cpp
	$(CXX) $(CXXFLAGS) -DHEESCH_BACKEND_CADICAL -c -o sat-cadical.o sat.cpp

sat-cadical: $(CADICAL_OBJECTS)
	$(CXX) -o sat-cadical $(CADICAL_OBJECTS) $(LIBS) -lcadical

sat-kissat.o: sat.cpp
	$(CXX) $(CXXFLAGS) -DHEESCH_BACKEND_KISSAT -c -o sat-kissat.o sat.cpp

sat-kissat: $(KISSAT_OBJECTS)
	$(CXX) -o sat-kissat $(KISSAT_OBJECTS) $(LIBS) -lkissat -lm

sat-dump.o: sat.cpp
	$(CXX) $(CXXFLAGS) -DHEESCH_BACKEND_DUMP -c -o sat-dump.o sat.cpp

sat-dump: $(DUMP_OBJECTS)
	$(CXX) -o sat-dump $(DUMP_OBJECTS) $(LIBS)

sat-wcnf-dump.o: sat.cpp
	$(CXX) $(CXXFLAGS) -DHEESCH_BACKEND_WCNF_DUMP -c -o sat-wcnf-dump.o sat.cpp

sat-wcnf-dump: $(WCNF_DUMP_OBJECTS)
	$(CXX) -o sat-wcnf-dump $(WCNF_DUMP_OBJECTS) $(LIBS)

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
	rm -f ${OBJECTS} ${DEPENDS} \
	      sat-cadical.o sat-cadical.d sat-kissat.o sat-kissat.d \
	      sat-dump.o sat-dump.d sat-wcnf-dump.o sat-wcnf-dump.d \
	      sat viz gen surrounds report \
	      sat-cadical sat-kissat sat-dump sat-wcnf-dump
