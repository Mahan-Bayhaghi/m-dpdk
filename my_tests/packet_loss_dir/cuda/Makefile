# Define compiler and flags
CC = nvcc
CFLAGS = $(shell pkg-config --cflags libdpdk) -I/usr/local/cuda/include
LDFLAGS = $(shell pkg-config --libs libdpdk) -L/usr/local/cuda/lib64 -lcudart

# Name of the output binary
TARGET = packet_loss

# Source file
SRCS = packet_loss.cu

# Default rule to build the program
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) -o $(TARGET) $(SRCS) $(CFLAGS) $(LDFLAGS)

# Clean rule to remove the compiled binary
clean:
	rm -f $(TARGET)
