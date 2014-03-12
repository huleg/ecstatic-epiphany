TARGET = imprint
CPP_FILES = \
	src/main.cpp

# Important optimization options
CPPFLAGS = -O3 -ffast-math

# Standard libraries
LDFLAGS = -lm -lstdc++

# Debugging
CPPFLAGS += -g -Wall -Wno-tautological-constant-out-of-range-compare -Wno-gnu-static-float-init
LDFLAGS += -g

# Dependency generation
CPPFLAGS += -MMD

OBJS := $(CPP_FILES:.cpp=.o) 

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $< -o $@ $(LFLAGS)

-include $(OBJS:.o=.d)

.PHONY: clean all

clean:
	rm -f $(TARGET) $(OBJS) $(OBJS:.o=.d)
