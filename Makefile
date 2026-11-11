# GNU Make, a C++20 GCC/Clang toolchain, and Python 3 for the test runner.
CXX ?= g++
PYTHON ?= python3
BUILD_DIR ?= build-make
CXXFLAGS ?= -O2 -g
PREFIX ?= /usr/local
DESTDIR ?=
EXEEXT ?= $(if $(filter Windows_NT,$(OS)),.exe,)
SANITIZE_CXX ?= clang++
ITERATIONS ?= 1000000

BASE_FLAGS := -std=c++20 -Wall -Wextra -Wpedantic
# An indirect reference keeps `make -n test` from executing the Python runner.
TEST_MAKE := $(MAKE)
HEADERS := $(wildcard include/flexmodel/*.hpp examples/*.hpp)
EXAMPLES := $(patsubst examples/%.cpp,$(BUILD_DIR)/%$(EXEEXT),$(wildcard examples/*.cpp))
TESTS := $(patsubst tests/%.cpp,$(BUILD_DIR)/%$(EXEEXT),$(wildcard tests/test_*.cpp))
BENCHMARK := $(BUILD_DIR)/dispatch_benchmark$(EXEEXT)

.PHONY: all examples test benchmark sanitize model-check install clean help
all: examples
examples: $(EXAMPLES)

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/%$(EXEEXT): examples/%.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BASE_FLAGS) -Iinclude "$<" -o "$@" $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/test_%$(EXEEXT): tests/test_%.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BASE_FLAGS) -Iinclude "$<" -o "$@" $(LDFLAGS) $(LDLIBS)

$(BENCHMARK): benchmarks/dispatch_benchmark.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BASE_FLAGS) -Iinclude -Iexamples "$<" -o "$@" $(LDFLAGS) $(LDLIBS)

test: examples $(TESTS)
	$(PYTHON) scripts/check.py --build-dir="$(BUILD_DIR)" --cxx="$(CXX)" \
	  --cxxflags="$(CPPFLAGS) $(CXXFLAGS) $(BASE_FLAGS)" --ldflags="$(LDFLAGS) $(LDLIBS)" \
	  --exeext="$(EXEEXT)" --make="$(TEST_MAKE)"

benchmark: $(BENCHMARK)
	"$(BENCHMARK)" "$(ITERATIONS)"

sanitize:
	$(MAKE) test BUILD_DIR="$(BUILD_DIR)-sanitize" CXX="$(SANITIZE_CXX)" \
	  CXXFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined"

model-check:
	bash tla/check.sh

install:
	install -d "$(DESTDIR)$(PREFIX)/include/flexmodel"
	cp include/flexmodel/*.hpp "$(DESTDIR)$(PREFIX)/include/flexmodel/"

clean:
	@test -n "$(BUILD_DIR)" && test "$(BUILD_DIR)" != / && test "$(BUILD_DIR)" != .
	rm -rf -- "$(BUILD_DIR)"

help:
	@printf '%s\n' \
	  'make                 Build all examples in build-make/' \
	  'make test            Run behavioral, allocation, planner, negative, CSV, and install checks' \
	  'make benchmark       Run the optional benchmark; ITERATIONS=1000000 by default' \
	  'make sanitize        Run tests with Clang ASan/UBSan in BUILD_DIR-sanitize/' \
	  'make model-check     Run bounded TLA+ checks (requires Java 17+, Bash, and curl)' \
	  'make install         Copy headers to DESTDIR/PREFIX/include; PREFIX=/usr/local by default' \
	  'make clean           Remove the selected BUILD_DIR' \
	  'Variables: CXX, BUILD_DIR, CPPFLAGS, CXXFLAGS, LDFLAGS, LDLIBS, PYTHON, EXEEXT, SANITIZE_CXX' \
	  'Use separate BUILD_DIR values when switching compilers or build flags.'
