CC=g++
CFLAGS=-c -Wall -Wpedantic -g -O0
LDFLAGS=
SOURCES=main.cpp workflow.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=lab

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
	rm $(EXECUTABLE)
