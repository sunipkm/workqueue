CC = gcc
CXX = g++
EDCFLAGS = -O2 -Wall $(CFLAGS)
EDCXXFLAGS = -O2 -Wall -std=c++11 $(CXXFLAGS)
EDLDFLAGS = -lpthread

COBJS = test.o \
		workqueue_posix.o

all: $(COBJS)
	$(CC) -o test.out $(COBJS) $(EDLDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(EDCFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(EDCXXFLAGS)

.PHONY: clean

clean:
	rm -vf *.o
	rm -vf *.out