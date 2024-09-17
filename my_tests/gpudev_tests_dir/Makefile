
CC = nvcc
CFLAGS = $(shell pkg-config --cflags libdpdk)
LDFLAGS = $(shell pkg-config --libs libdpdk)

# Name of the output binary
TARGET = simple_dpdk_gpu

# Source file
SRCS = simple_dpdk_gpu.cu

# Default rule to build the program
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) -o $(TARGET) $(SRCS) $(CFLAGS) $(LDFLAGS)

# Clean rule to remove the compiled binary
clean:
	rm -f $(TARGET)
