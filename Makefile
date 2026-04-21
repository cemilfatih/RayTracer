CXX       = clang++
CXXFLAGS  = -std=c++17 -O3 -Wall -Wextra -pthread -Iinclude -Iexternal
LDFLAGS   = -pthread

SRCS      = $(wildcard src/*.cpp) external/tinyxml2.cpp
OBJS      = $(SRCS:.cpp=.o)
TARGET    = raytracer

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean