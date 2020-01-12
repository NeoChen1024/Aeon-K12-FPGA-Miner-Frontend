CC=	cc
CXX=	c++
CFLAGS=	-Ofast -march=native -g3 -Wall -Wextra -pipe -Isrc -pthread -std=c++17
CXXFLAGS= $(CFLAGS)
LIBS=	-lvulkan
OBJS=	src/config.o src/httpConsole.o src/k12CpuMiner.o src/log.o src/main.o \
	src/miner.o src/mvulkan.o src/network.o src/slow_hash.o src/spirv.o

.PHONY: all clean

all:	miner
miner:	$(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o miner $(LIBS)

clean:
	rm -fv $(OBJS) miner
