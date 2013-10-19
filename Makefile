CFLAGS += -Wall -Wextra
CFLAGS += -Wno-unused -Wno-unused-parameter
CFLAGS += -std=c11
all: main
main: tendryl.o
main.o tendryl.o: tendryl.h
clean:
	$(RM) *.o main

