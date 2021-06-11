
CC?=clang-9

SOURCE=../DashHashTable
EXECUTABLE=DHT

DEBUG?=0
DEBUG_FLAGS=-O0 -g3 -D__TA_DEBUG=$(DEBUG)

INCLUDE=-I../../include/
LIBRARIES=-lm

simp: $(EXECUTABLE).simple

indexClash: $(EXECUTABLE).indexClash

clashResolve: $(EXECUTABLE).clashResolve

io: $(EXECUTABLE).IO

all: simp indexClash clashResolve io

$(EXECUTABLE).simple: $(SOURCE).c
	$(CC) $(DEBUG_FLAGS) $(INCLUDE) $(LIBRARIES) simple.c $< -o $@

$(EXECUTABLE).indexClash: $(SOURCE).c
	$(CC) $(DEBUG_FLAGS) $(INCLUDE) $(LIBRARIES) IndexClash.c $< -o $@

$(EXECUTABLE).clashResolve: $(SOURCE).c
	$(CC) $(DEBUG_FLAGS) $(INCLUDE) $(LIBRARIES) ClashResolveAccuracy.c $< -o $@

$(EXECUTABLE).IO: $(SOURCE).c
	$(CC) $(DEBUG_FLAGS) $(INCLUDE) $(LIBRARIES) IO.c $< -o $@

clean: 
	rm -rf $().exec $(EXECUTABLE).simple $(EXECUTABLE).indexClash $(EXECUTABLE).clashResolve $(EXECUTABLE).IO *.bin
