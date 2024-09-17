# Define compiler and flags
CC = gcc
CFLAGS = $(shell pkg-config --cflags libdpdk)
LDFLAGS = $(shell pkg-config --libs libdpdk)

# Name of the output binary
TARGET = packet_loss

# Source file
SRCS = packet_loss.c

# Default rule to build the program
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) -o $(TARGET) $(SRCS) $(CFLAGS) $(LDFLAGS)

# Clean rule to remove the compiled binary
clean:
	rm -f $(TARGET)
