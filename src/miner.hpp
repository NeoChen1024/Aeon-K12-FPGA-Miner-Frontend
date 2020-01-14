#ifndef MINER_HPP_
#define MINER_HPP_

#include "config.hpp"
#include "network.hpp"

void *K12CpuMinerThread(void *args);

void incGoodHash(int gpuIndex);
void incBadHash(int gpuIndex);
int getGoodHash(int gpuIndex);
int getBadHash(int gpuIndex);
uint64_t getHashRates(int index);
void setHashRates(int index, uint64_t v);

#endif /* MINER_HPP_ */
