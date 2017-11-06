CC=gcc
CFLAGS=-c -Wall -Wpedantic -g -O0
LDFLAGS=
SOURCES=main.c workflow.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=lab

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm $(EXECUTABLE)
