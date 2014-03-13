TARGET = imprint
CPP_FILES = \
	src/main.cpp \
	src/lib/camera_somagic.cpp \
	src/lib/jpge.cpp

# Important optimization options
CPPFLAGS = -O3 -ffast-math

# Libraries
LDFLAGS = -lm -lstdc++ -lusb-1.0

# Debugging
CPPFLAGS += -g -Wall -Wno-tautological-constant-out-of-range-compare -Wno-gnu-static-float-init
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
