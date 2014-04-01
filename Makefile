TARGET = ei
CPP_FILES = \
	src/main.cpp \
	src/narrator.cpp \
	src/lib/camera_somagic.cpp \
	src/lib/jpge.cpp \
	src/lib/lodepng.cpp

# Use a recent toolchain (Linux)
CXX := gcc-4.8 -std=c++11 -fpermissive

# Important optimization options
CPPFLAGS = -O3 -ffast-math

# Libraries
LDFLAGS = -lm -lstdc++ -lusb-1.0

# SDL (For debugging)
CPPFLAGS += $(shell sdl-config --cflags)
LDFLAGS += $(shell sdl-config --libs)

# Debugging
CPPFLAGS += -g -Wall
LDFLAGS += -g

# Dependency generation
CPPFLAGS += -MMD

OBJS := $(CPP_FILES:.cpp=.o) 

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

-include $(OBJS:.o=.d)

.PHONY: clean all

clean:
	rm -f $(TARGET) $(OBJS) $(OBJS:.o=.d)
