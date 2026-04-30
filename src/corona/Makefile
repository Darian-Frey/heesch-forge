# heesch-forge corona-completion oracle (M2.2).
#
# Builds libcorona.a and the test runner test_corona. Depends on the
# DLX library at ../dlx/libdlx.a; if that does not exist, this Makefile
# builds it via a recursive `make -C ../dlx`. `make test` is the M2.2
# acceptance gate.

CXX      = g++
OPT      = -O3 -DNDEBUG
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -MMD $(OPT) -I.

DLX_DIR     = ../dlx
DLX_LIB     = $(DLX_DIR)/libdlx.a

LIB_OBJECTS  = oracle.o corona_state.o
LIB_DEPENDS  = ${LIB_OBJECTS:.o=.d}

TEST_OBJECTS = tests/test_corona.o
TEST_DEPENDS = ${TEST_OBJECTS:.o=.d}

.PHONY: all test clean dlx-lib

all: libcorona.a test_corona

# Recursive build of the DLX library if it is missing or stale.
$(DLX_LIB):
	$(MAKE) -C $(DLX_DIR) libdlx.a

dlx-lib: $(DLX_LIB)

libcorona.a: $(LIB_OBJECTS)
	ar rcs $@ $(LIB_OBJECTS)

test_corona: $(TEST_OBJECTS) libcorona.a $(DLX_LIB)
	$(CXX) -o $@ $(TEST_OBJECTS) libcorona.a $(DLX_LIB)

test: test_corona
	./test_corona

-include ${LIB_DEPENDS} ${TEST_DEPENDS}

clean:
	rm -f $(LIB_OBJECTS) $(LIB_DEPENDS) \
	      $(TEST_OBJECTS) $(TEST_DEPENDS) \
	      libcorona.a test_corona
