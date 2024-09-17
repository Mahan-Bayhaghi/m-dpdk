# Define the target executable
TARGET = first_test

# Define the source files
SRCS = first_test.c

# Use the pkg-config tool to get DPDK compile and link flags
CFLAGS = $(shell pkg-config --cflags libdpdk)
LDFLAGS = $(shell pkg-config --libs libdpdk)

# Define the build target
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Define a clean target to remove the build artifacts
clean:
	rm -f $(TARGET)
