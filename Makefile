CXX ?= g++
CPPFLAGS ?= -Iinclude
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

TARGET := dmlcs
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
DEP := $(OBJ:.o=.d)

ifeq ($(OS),Windows_NT)
TARGET := dmlcs.exe
LDLIBS += -lpsapi
endif

.PHONY: all clean debug
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

debug: clean
	$(MAKE) CXXFLAGS="-std=c++17 -O0 -g -Wall -Wextra -Wpedantic" all

clean:
	$(RM) $(OBJ) $(DEP) dmlcs dmlcs.exe

-include $(DEP)
