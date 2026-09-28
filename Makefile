CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude -Isrc
TARGET := is_mpl
SOURCES := src/io.cpp src/main.cpp src/solver.cpp

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SOURCES) include/is_mpl/metrics.h include/is_mpl/solver.h \
           src/graph_types.h src/io.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) -o $@ $(LDLIBS)

test: $(TARGET)
	python tests/test_cli.py ./$(TARGET)

clean:
	rm -f $(TARGET)
