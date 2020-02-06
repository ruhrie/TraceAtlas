
CC?=clang-9
CXX?=clang++-9

DASH_ROOT?=/mnt/nobackup-09/Dash/Sources/

INCLUDE=-I$(DASH_ROOT)include/
LIBS=$(DASH_ROOT)lib/libAtlasBackend.a

TRACING=0

all:
	$(CC) $(INCLUDE) -lz main.c -o matmul.exec $(LIBS) -DTRACING=$(TRACING)

trace:
	$(CC) $(LDFLAGS) $(INCLUDE) main.c -o matmul.bc -DTRACING=$(TRACING)
	opt-9 -load $(DASH_ROOT)/lib/AtlasPasses.so -EncodedTrace matmul.bc -o matmul.tra
	$(CXX) -lz -lpthread -fuse-ld=lld-9 matmul.tra -o matmul.native $(LIBS)
	./matmul.native

clean:
	rm -rf *.bc *.tr* *.native
