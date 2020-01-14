CC=	cc
CXX=	c++
CFLAGS=	-Ofast -march=native -g3 -Wall -Wextra -pipe -Isrc -pthread -std=c++17 -flto
CXXFLAGS= $(CFLAGS)
LIBS=
OBJS=	src/config.o src/k12CpuMiner.o src/log.o src/main.o src/miner.o src/network.o

.PHONY: all clean

all:	miner
miner:	$(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o miner $(LIBS)

clean:
	rm -f $(OBJS) miner
