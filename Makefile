CC = gcc
CFLAGS = -Wall -Wshadow -Wvla -g
TARGET = shell2.0.c
OBJECTS = shell2.0.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -g -o $(TARGET) $(OBJECTS)

shell2.0.o: shell2.c
	$(CC) $(CFLAGS) -c shell2.c

clean:
	rm -f $(TARGET) $(OBJECTS)
