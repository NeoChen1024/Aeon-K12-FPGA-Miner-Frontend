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
#include <string.h>
#include <iostream>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#ifdef __MINGW32__
#include <ws2tcpip.h>
#include <winsock.h>
#include <winsock2.h>
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#endif
#include <unistd.h>
#include <iomanip>

#include "config.hpp"
#include "log.hpp"
#include "miner.hpp"
#include "network.hpp"
#include "miner.hpp"

using namespace std;

#ifdef __MINGW32__
static const char *START_STUCKED="";
static const char *START_GREEN="";
static const char *START_LIGHT_GREEN = "";
static const char *START_WHITE="";
#define THREAD_HANDLE HANDLE
#else
static const char *START_STUCKED = "\e[91m";
static const char *START_GREEN = "\e[32m";
static const char *START_LIGHT_GREEN = "\e[92m";
static const char *START_WHITE = "\e[39m";
#define THREAD_HANDLE pthread_t
#endif

static 	CPUMiner cpuMiners[MAX_CPUS];

#ifdef __MINGW32__
int getKey() {
	if(kbhit()) return getch();
	else return 0;
}
#else
int getKey() {
	struct termios oldt, newt;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	int ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch != EOF)
		return ch;
	else
		return 0;
}
#endif

int main(int argc, char **argv) {
#ifdef __MINGW32__
	// init windows tcp sockets
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		error("Error initialising WSA.",NULL);
	}
#endif
	cout << "Vulkan SPIR-V XMR Miner " << VERSION << "\n\n" << flush;
#ifdef __MINGW32__
	cout << "\nIf you experience very low hashrate, close apps like firefox, chrome,..., start the miner, and then reopen them.\n";
#endif
	if (!checkConfig())
		makeConfig();

	readConfig();

	CPUMiner cpuMiner;
	memset(&cpuMiner, 0, sizeof(cpuMiner));
	cpuMiner.debugNetwork = config.debugNetwork;

	initNetwork(cpuMiner);
	initMiners();

	registerPool(config.poolAddress,config.poolPort,config.address, config.poolPassword);
	if (lookForPool())
		connectToPool();

	startNetworkBG();

	while (getVariant() == 0) {
		cout << "Waiting for pool to send blob....\n";
		sleep(5);
	}
	cout << "Using XMR Variant: KangarooTwelve\n";

	THREAD_HANDLE threads[MAX_CPUS];

	for (int i = 0; i < config.nbCPUs; i++) {
		cpuMiners[i] = cpuMiner;
		cpuMiners[i].index = i;
#ifdef __MINGW32__
		DWORD ThreadId;
		threads[i] = CreateThread(NULL,0,K12CpuMinerThread, &cpuMiners[i],0,&ThreadId);
#else
		pthread_create(&threads[i], NULL, K12CpuMinerThread, &cpuMiners[i]);
#endif
	}

	cout << "Mining started.... (Press q to stop)\n";
	cout << "[Time] 'Total H/s' 'Good shares'/'Invalid shares'/'Expired shares' [GPU] H/s 'good hashes'/'bad hashes' \n";
	sleep(10);
	bool quit = false;

	float mHashrate[MAX_CPUS];
	int stucked[MAX_CPUS];
	for (int i = 0; i < config.nbCPUs; i++) {
		mHashrate[i] = 0;
		stucked[i] = 0;
	}
	int loop = 0;

	while (!quit) {
		char c = getKey();
		quit = c == 'q';

		for (int i = 0; i < config.nbCPUs; i++) {
			if (mHashrate[i] == 0)
				mHashrate[i] = getHashRates(i);

			if (getHashRates(i) != 0) {
				mHashrate[i] = 0.95 * mHashrate[i] + 0.05 * getHashRates(i);
				stucked[i] = 0;
			} else
				stucked[i]++;

			if (stucked[i] > 30)
				mHashrate[i] = 0;			//  stucked
		}
		if (loop == config.consoleRefreshRate) {
			cout << START_GREEN;
			printDate();
			cout.unsetf(std::ios::floatfield);
			cout << fixed;
			cout.precision(3);
			float hashesPerSec = 0;
			for (int i = 0; i < config.nbCPUs; i++)
				hashesPerSec += (mHashrate[i] / 1000.0);

			int totalShares = 0;
			for (int i = 0; i < config.nbCPUs; i++)
				totalShares += getGoodHash(i);
			for (int i = 0; i < config.nbCPUs; i++)
				totalShares += getGoodHash(i);

			if (hashesPerSec < 3e4)
				cout << START_LIGHT_GREEN << hashesPerSec << "H/s " << START_GREEN << totalShares << "/" << getInvalidShares() << "/" << getExpiredShares() << " ";
			else if (hashesPerSec < 1e6)
				cout << START_LIGHT_GREEN << hashesPerSec/1000 << "kH/s " << START_GREEN << totalShares << "/" << getInvalidShares() << "/" << getExpiredShares() << " ";
			else
				cout << START_LIGHT_GREEN << hashesPerSec/1000000 << "MH/s " << START_GREEN << totalShares << "/" << getInvalidShares() << "/" << getExpiredShares() << " ";
			for (int i = 0; i < config.nbCPUs; i++) {
				if (mHashrate[i] < 3e7)
					cout << "[" << i << "]:" << (mHashrate[i] == 0 ? START_STUCKED : "") << (mHashrate[i] / 1000.0) << START_GREEN << "H/s " << getGoodHash(i) << "/" << getBadHash(i) << "  ";
				else if (mHashrate[i] < 1e9)
					cout << "[" << i << "]:" << (mHashrate[i] == 0 ? START_STUCKED : "") << (mHashrate[i] / 1000000.0) << START_GREEN << "kH/s " << getGoodHash(i) << "/" << getBadHash(i) << "  ";
				else
					cout << "[" << i << "]:" << (mHashrate[i] == 0 ? START_STUCKED : "") << (mHashrate[i] / 1000000000.0) << START_GREEN << "Mh/s " << getGoodHash(i) << "/" << getBadHash(i) << "  ";
			}
			cout << START_WHITE << "\n";
			loop = 1;
		} else
			loop++;

		cout << flush;
		sleep(1);
	}

	requestStop();
	cout << "Shutdown requested....\n";
	for (int i = 0; i < config.nbCPUs; i++)
#ifdef __MINGW32__
		WaitForSingleObject(threads[i],INFINITE);
#else
		pthread_join(threads[i], NULL);
#endif

#ifdef __MINGW32__
	WSACleanup();
#endif
	return 0;
}
