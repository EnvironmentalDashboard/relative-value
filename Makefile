CC=gcc
# https://stackoverflow.com/questions/1778538/how-many-gcc-optimization-levels-are-there
# -ggdb3 is for valgrind, -g option is for gdb debugger; remove in production
CFLAGS=-Og -pedantic -std=c99 -Wall -Wextra -ggdb3 -std=gnu11
LFLAGS=-lcurl
MYSQL_CONFIG=`mysql_config --cflags --libs`

all: main

main: main.o relative_value.o lib/cJSON/cJSON.o
	$(CC) $(CFLAGS) main.o relative_value.o lib/cJSON/cJSON.o $(MYSQL_CONFIG) -o main $(LFLAGS)

main.o: main.c relative_value.h
	$(CC) $(CFLAGS) -c main.c $(MYSQL_CONFIG) $(LFLAGS)

relative_value.o: relative_value.c relative_value.h
	$(CC) $(CFLAGS) -c relative_value.c

clean:
	rm main *.o
