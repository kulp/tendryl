CFLAGS += -Wall -Wextra
CFLAGS += -Wno-unused -Wno-unused-parameter
CFLAGS += -std=c11
all: main Test.class
main: tendryl.o
main.o tendryl.o: tendryl.h
%.class: %.java
	javac $<
clean:
	$(RM) *.o main

