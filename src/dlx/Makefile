# heesch-forge DLX library + tests (M2.1).
#
# Builds the standalone DLX library at libdlx.a and the test runner
# at test_dlx. The library has no external dependencies; it links
# against nothing beyond libstdc++. Pulled into other heesch-forge
# subsystems by adding -I$(REPO)/src/dlx and linking libdlx.a.

CXX      = g++
OPT      = -O3 -DNDEBUG
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -MMD $(OPT) -I.

LIB_OBJECTS  = dlx.o
LIB_DEPENDS  = ${LIB_OBJECTS:.o=.d}

TEST_OBJECTS = tests/test_dlx.o
TEST_DEPENDS = ${TEST_OBJECTS:.o=.d}

.PHONY: all test clean

all: libdlx.a test_dlx

libdlx.a: $(LIB_OBJECTS)
	ar rcs $@ $(LIB_OBJECTS)

test_dlx: $(TEST_OBJECTS) libdlx.a
	$(CXX) -o $@ $(TEST_OBJECTS) libdlx.a

# Run the test suite. `make test` is the M2.1 acceptance gate; non-zero
# exit means a regression in DLX correctness.
test: test_dlx
	./test_dlx

-include ${LIB_DEPENDS} ${TEST_DEPENDS}

clean:
	rm -f $(LIB_OBJECTS) $(LIB_DEPENDS) \
	      $(TEST_OBJECTS) $(TEST_DEPENDS) \
	      libdlx.a test_dlx
