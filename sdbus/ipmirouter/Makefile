

CC?=gcc
IDIR =. 
CFLAGS=-I$(IDIR) $(shell pkg-config --cflags --libs libsystemd)

DEPS = debug.h 



OBJ = ipmiSDbus.o debug.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ipmirouter: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

