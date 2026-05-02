# M2.4 hybrid-pipeline harness.
#
# Builds run_hybrid, which links libcorona.a + libdlx.a from
# ../../src/corona/ and ../../src/dlx/. `make test-n7` runs the
# pipeline on the n=7 batch as a quick smoke check; `make test-small`
# covers n in {7, 8, 12} (174 - 110 = 64 shapes, manageable).

CXX      = g++
OPT      = -O3 -DNDEBUG
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -MMD $(OPT) \
            -I../../src/corona -I../../src/dlx

CORONA_LIB = ../../src/corona/libcorona.a
DLX_LIB    = ../../src/dlx/libdlx.a

OBJECTS = run_hybrid.o bounded_dlx.o
DEPENDS = ${OBJECTS:.o=.d}

DATA_DIR = ../../data/kaplan-2022/omino

.PHONY: all clean libs test-n7 test-small test-full

all: run_hybrid bounded_dlx

libs:
	$(MAKE) -C ../../src/dlx libdlx.a
	$(MAKE) -C ../../src/corona libcorona.a

$(CORONA_LIB) $(DLX_LIB):
	$(MAKE) libs

run_hybrid: run_hybrid.o $(CORONA_LIB) $(DLX_LIB)
	$(CXX) -o $@ run_hybrid.o $(CORONA_LIB) $(DLX_LIB)

bounded_dlx: bounded_dlx.o $(CORONA_LIB) $(DLX_LIB)
	$(CXX) -o $@ bounded_dlx.o $(CORONA_LIB) $(DLX_LIB)

test-n7: run_hybrid
	./run_hybrid $(DATA_DIR)/07omino_0up.txt > results/m2.4-hybrid-n7.csv
	@echo "# results/m2.4-hybrid-n7.csv written"

test-small: run_hybrid
	for n in 07 08 12; do \
		./run_hybrid $(DATA_DIR)/$${n}omino_*up.txt \
			> results/m2.4-hybrid-n$${n}.csv ; \
	done
	@echo "# results/m2.4-hybrid-n0[78,12].csv written"

test-full: run_hybrid
	for n in 07 08 11 12; do \
		./run_hybrid $(DATA_DIR)/$${n}omino_*up.txt \
			> results/m2.4-hybrid-n$${n}.csv ; \
	done
	@echo "# results/m2.4-hybrid-n0[7,8,11,12].csv written"

-include ${DEPENDS}

clean:
	rm -f $(OBJECTS) $(DEPENDS) run_hybrid
