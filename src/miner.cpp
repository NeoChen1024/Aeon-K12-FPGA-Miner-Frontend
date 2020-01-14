/*
 Cryptonight Vulkan Mining Software
 Copyright (C) 2019  enerc

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <inttypes.h>
#include <iostream>

#include "config.hpp"
#include "log.hpp"

static uint32_t goodHash[MAX_CPUS];
static uint32_t badhash[MAX_CPUS];
static uint64_t hashRates[MAX_CPUS];

void initMiners() {
}

// not thread safe, but don't care if we loose one
void incGoodHash(int gpuIndex) {
	goodHash[gpuIndex]++;
}

// not thread safe, but don't care if we loose one
void incBadHash(int gpuIndex) {
	badhash[gpuIndex]++;
}


int getGoodHash(int gpuIndex) {
	return goodHash[gpuIndex];
}

int getBadHash(int gpuIndex) {
	return badhash[gpuIndex];
}

uint64_t getHashRates(int index) {
	return hashRates[index];
}

void setHashRates(int index, uint64_t v) {
	hashRates[index] = v;
}
