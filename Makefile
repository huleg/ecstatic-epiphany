TARGET = ei
CPP_FILES = \
	src/main.cpp \
	src/narrator.cpp \
	src/narrator_script.cpp \
	src/lib/camera_somagic.cpp \
	src/lib/jpge.cpp \
	src/lib/lodepng.cpp

UNAME := $(shell uname)

# Important optimization options
CPPFLAGS = -O3

# Libraries
LDFLAGS = -lm -lstdc++ -lusb-1.0 -lopencv_core -lopencv_video

# Debugging
CPPFLAGS += -g -Wall
LDFLAGS += -g

# Dependency generation
CPPFLAGS += -MMD

ifeq ($(UNAME), Linux)
	# Use a recent toolchain (Linux)
	CXX := gcc-4.8 -std=c++11
	CPPFLAGS += -march=native
	LDFLAGS += -march=native
	CPPFLAGS += -pthread
	LDFLAGS += -pthread
endif

ifeq ($(UNAME), Darwin)
	CPPFLAGS += -Wno-gnu-static-float-init
endif

OBJS := $(CPP_FILES:.cpp=.o) 

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

-include $(OBJS:.o=.d)

.PHONY: clean all

clean:
	rm -f $(TARGET) $(OBJS) $(OBJS:.o=.d)
