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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <iostream>


#include "config.hpp"
#include "log.hpp"
#include "miner.hpp"
#include "network.hpp"

#define MAX_CARDS	  32
#define MAX_WALLET_SIZE 256
#define MAX_HOSTNAME_SIZE 256
#define MAX_INVALID_SHARES 10
#define MAX_ID_SIZE		   64
#define QUEUE_SIZE 		64
#define NONCE_LOCATION	39

static const char *START_YELLOW ="\e[33m";
static const char *START_WHITE = "\e[39m";
static const char *START_RED=	 "\e[91m";

static int connections;
static char hexBlob[MAX_BLOB_SIZE];
static unsigned char blob[MAX_BLOB_SIZE / 2];
static volatile char jobId[MAX_ID_SIZE];
static volatile char myIds[2][MAX_ID_SIZE];
static volatile int blobSize;
static volatile uint64_t target;
static volatile uint64_t height;
static bool stopRequested;
static int current_index;
static uint64_t mpool, dpool;
static int expiredShares;
static char hostname[MAX_HOSTNAME_SIZE];
static int port;
static char wallet[MAX_WALLET_SIZE];
static char password[MAX_WALLET_SIZE];
static int invalidShares;
static int successiveInvalidShares;
static const char CONVHEX[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

// Network queue
static pthread_t thread_id;
static sem_t mutex;
static sem_t mutexQueue;
static int tail;
static int head;
struct MsgResult {
	int64_t nonce;
	int cardIndex;
	unsigned char hash[64];
	unsigned char blob[MAX_BLOB_SIZE / 2];
};
static struct MsgResult msgResult[QUEUE_SIZE];
static const uint64_t TimeRotate = 60LL*100LL*1000LL*1000LL;
static bool debugNetwork;
static float hashesPerSec;
static uint32_t totalShares;

using namespace std;

//	Get ip from domain name
static bool hostname_to_ip(const char *hostname, char* ip) {
	struct hostent *he;
	struct in_addr **addr_list;

	if ((he = gethostbyname(hostname)) == NULL)
		return false;

	addr_list = (struct in_addr **) he->h_addr_list;

	for (int i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ip, (const char*) inet_ntoa(*addr_list[i]));
		return true;
	}

	return false;
}

static uint64_t now(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t) tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

static bool isRetStatusOk(const char *msg) {
	const char *needle = "\"status\":\"";
	const int needleLen = strlen(needle);

	const char *loc = strstr(msg, needle);
	if (loc != NULL && (loc[needleLen] == 'O' && loc[needleLen + 1] == 'K')) {
		return true;
	}

	const char *needle2 = "Your IP is banned";
	loc = strstr(msg, needle2);
	if (loc != NULL) {
		cout << START_RED << "Your IP is banned" << START_WHITE << "\n";
	} else
		cout << START_RED << "Login fails: " << msg << START_WHITE << "\n";

	connections = 0;
	return false;
}

static int hex2c(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return 0; // ??
}

static void hex2bin(const char* in, unsigned int len, unsigned char* out) {
	for (unsigned int i = 0; i < len; i += 2)
		out[i / 2] = (hex2c(in[i]) << 4) | hex2c(in[i + 1]);
}

static void bin2hex(const unsigned char* in, unsigned int len, unsigned char* out) {
	for (unsigned int i = 0; i < len; i++) {
		out[i * 2] = CONVHEX[in[i] >> 4];
		out[i * 2 + 1] = CONVHEX[in[i] & 0xf];
	}
}

static bool getBlob(const char *msg) {
	const char *needle = "\"blob\":\"";
	const int needleLen = strlen(needle);

	const char *loc = strstr(msg, needle);
	if (loc == NULL)
		return false;
	loc += needleLen;

	int i = 0;
	while (*loc != '"') {
		hexBlob[i++] = *loc;
		loc++;
	}
	hexBlob[i++] = 0;
	assert(i < MAX_BLOB_SIZE);

	sem_wait(&mutex);

	memset(blob,0,MAX_BLOB_SIZE/2);
	// convert from hex representation
	for (int j = 0; j < i / 2; j++) {
		int k = hex2c(hexBlob[j * 2]) << 4;
		k += hex2c(hexBlob[j * 2 + 1]);
		blob[j] = k;
	}
	blobSize = i / 2;
	sem_post(&mutex);

	return true;
}

static bool getJobId(const char *msg) {
	const char *needle = "\"job_id\":\"";
	const int needleLen = strlen(needle);

	const char *loc = strstr(msg, needle);
	if (loc == NULL)
		return false;
	loc += needleLen;

	int i = 0;
	while (*loc != '"') {
		jobId[i++] = *loc;
		loc++;
	}
	jobId[i++] = 0;
	return true;
}

static bool getMyId(const char *msg) {
	const char *needle = "\"result\":{\"id\":\"";
	const int needleLen = strlen(needle);

	const char *loc = strstr(msg, needle);
	if (loc == NULL)
		return false;
	loc += needleLen;

	int i = 0;
	while (*loc != '"') {
		myIds[current_index][i++] = *loc;
		loc++;
	}
	myIds[current_index][i++] = 0;
	return true;
}

static bool decodeTarget(const char *msg) {
	const char *needle = "\"target\":\"";
	const int needleLen = strlen(needle);

	const char *loc = strstr(msg, needle);
	if (loc == NULL)
		return false;
	loc += needleLen;

	int i = 0;
	char tmp[MAX_TARGET_SIZE];
	memset(tmp, '0', MAX_TARGET_SIZE);
	while (*loc != '"') {
		tmp[i] = *loc;
		i++;
		loc++;
	}
	uint64_t tmp_target = 0;
	hex2bin(tmp, 16, (unsigned char*) &tmp_target);
	target = tmp_target;						// atomic write
	return true;
}

static bool decodeHeight(const char *msg) {
	const char *needle = "\"height\":";
	const int needleLen = strlen(needle);

	const char *loc = strstr(msg, needle);
	if (loc == NULL) {
		return false;
	}
	loc += needleLen;

	int i = 0;
	char tmp[20];
	while (*loc >= '0' && *loc <= '9' && i<20) {
		tmp[i] = *loc;
		i++;
		loc++;
	}
	tmp[i] = 0;
	height = atol(tmp);						// atomic write
	return true;
}

void getCurrentBlob(unsigned char *input, int *size) {
	sem_wait(&mutex);
	memset(input, 0, MAX_BLOB_SIZE/2);
	memcpy(input, blob, blobSize);
	input[blobSize] = 0x01;
	*size = blobSize;
	sem_post(&mutex);
}

void applyNonce(unsigned char *input, uint64_t nonce) {
	// add the nonce starting at pos 39.
	if (getVariant() == K12_ALGO) {		// K12_nonce64
		*(uint64_t *) (input + NONCE_LOCATION) = nonce;
	} else {
		*(uint32_t *) (input + NONCE_LOCATION) = (uint32_t)(nonce & 0xffffffff);
	}
}

void registerPool(const char* _hostname, int _port, const char *_wallet, const char *_password) {
	memcpy(hostname, _hostname, strlen(_hostname));
	hostname[strlen(hostname)] = 0;
	port = _port;
	memcpy(wallet, _wallet, strlen(_wallet));
	memcpy(password, _password, strlen(_password));
}

bool lookForPool() {
	char fullName[2048];
	sprintf(fullName, "%s:%d", hostname, port);

	// hostname to IP
	char ip[100];
	if (!hostname_to_ip(hostname, ip)) {
		char err[2048];
		sprintf(err, "Invalid mining pool hostname: %s\n", hostname);
		error(err,NULL);
		return false;
	}

	// Create socket
	int soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (soc < 0) {
		exitOnError("Can't create socket");
		return false;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	uint64_t arg;
	// Set non-blocking
	if ((arg = fcntl(soc, F_GETFL, NULL)) < 0) {
		error("Error fcntl(..., F_GETFL) ", strerror(errno));
		exitOnError("Can't continue");
		return false;
	}
	arg |= O_NONBLOCK;
	if (fcntl(soc, F_SETFL, arg) < 0) {
		error("Error fcntl(..., F_SETFL) ", strerror(errno));
		exitOnError("Can't continue");
		return false;
	}

	// Trying to connect with timeout
	errornc("Connecting to", fullName);
	errornc(" ... ", NULL);
	int res = connect(soc, (struct sockaddr *) &addr, sizeof(addr));
	if (res != 0) {
		struct timeval tv;
		tv.tv_sec = CONNECT_TIMEOUT;
		tv.tv_usec = 0;
		fd_set myset;
		FD_ZERO(&myset);
		FD_SET(soc, &myset);
		res = select(soc + 1, NULL, &myset, NULL, &tv);
		if (res == 0) {
			connections = 0;
			error("Timeout connecting to mining pool", fullName);
			return false;
		}
	}
	error("done!", "");

	// Set to blocking mode again...
	if ((arg = fcntl(soc, F_GETFL, NULL)) < 0) {
		error("Error fcntl(..., F_GETFL)", strerror(errno));
		exitOnError("");
	}
	arg &= (~O_NONBLOCK);
	if (fcntl(soc, F_SETFL, arg) < 0) {
		error("Error fcntl(..., F_SETFL)", strerror(errno));
		exitOnError("");
	}

	connections = soc;
	return true;
}

static void WaitForJob() {
	while (blob[0] == 0) {
		usleep(100);
	}
}

uint64_t getTarget() {
	WaitForJob();
	if (target != 0) {
		if (getVariant() != K12_ALGO)
			return 0xFFFFFFFFUL * target;
		else
			return target;
	} else
		return 0;
}

uint64_t getRandomNonce(int gpuIndex) {
	if (getVariant() != K12_ALGO)
		return gpuIndex * 5 * 3600 * 2000; // 5 hours at 2kH/s;
	else
		return uint64_t((uint64_t)gpuIndex * 5L * 3600L * 2000L)*1000L*1000L; // 5 hours at 2GH/s;
}

bool connectToPool() {
	assert(connections > 0);

	char msg[4096];
	sprintf(msg, "{\"method\":\"login\",\"params\":{\"login\":\"%s\",\"pass\":\"%s\",\"rigid\":\"\",\"agent\":\"%s\"},\"id\":1}\n", wallet, password, MINING_AGENT);

	if (debugNetwork && current_index == 0)
		cout << START_YELLOW << "SEND " << msg << START_WHITE;

	unsigned int sent = send(connections, msg, strlen(msg), 0);
	if (sent != strlen(msg)) {
		error("Connection lost during connection", "");
		connections = 0;
		return false;
	}
	int len = 0;

	struct timeval tv;
	tv.tv_sec = 5 * CONNECT_TIMEOUT;
	tv.tv_usec = 0;
	fd_set set;
	FD_ZERO(&set); 						// clear the set
	FD_SET(connections, &set); 	// add our file descriptor to the set

	int rv = select(connections + 1, &set, NULL, NULL, &tv);
	if (rv == -1) {
		error("Mining pools failed to respond", NULL);
		return false;
	} else if (rv == 0) {
		error("Mining pool timed out", "");
		return false;
	} else {
		len = read(connections, msg, 4096);
		msg[len] = 0;
	}
	if (debugNetwork && len > 0 && current_index == 0)
		cout << START_YELLOW << "RECV " << msg << START_WHITE;

	if (!isRetStatusOk(msg)) {
		return false;
	}

	if (!getBlob(msg)) {
		error("Fail to get blob", NULL);
		return false;
	}

	if (!getJobId(msg)) {
		error("Fail to get job_id", NULL);
		return false;
	}

	if (!getMyId(msg)) {
		error("Fail to get my id", NULL);
		return false;
	}

	if (!decodeTarget(msg)) {
		error("Fail to get target", NULL);
		return false;
	}

	// CryptonightR
	decodeHeight(msg);

	return true;
}

void closeConnection() {
	connections = 0;
}

static void checkNewBloc(const char *msg) {
	const char *needle = "\"blob\":\"";
	const char *loc = strstr(msg, needle);
	if (loc == NULL)
		return;

	getBlob(msg);
	getJobId(msg);
	decodeTarget(msg);
}

static void checkInvalidShare(const char *msg) {
	static const char *needle = "\"error\":null";
	const char *loc = strstr(msg, needle);
	if (loc != NULL) {
		successiveInvalidShares = 0;
		return;
	}

	static const char *needle2 = "Block expired";
	loc = strstr(msg, needle2);
	if (loc != NULL) {
		expiredShares++;
		return;
	}

	static const char *needle3 = "Low difficulty share";
	loc = strstr(msg, needle3);
	if (loc != NULL) {
		invalidShares++;
		//successiveInvalidShares++;
		cout << START_RED << "Result rejected by the pool.\n" << START_WHITE;
		return;
	}

	if (successiveInvalidShares > MAX_INVALID_SHARES) {
		closeConnection();
		error("Too many INVALID shares.", "Check your config or you will be BANNED!!");

		// avoid crazy connection loop on errors.
		sleep(20);

		// then reset the connection
		if (lookForPool())
			connectToPool();
	}
}

bool checkBlob(const unsigned char *_blob) {
	assert(blobSize < MAX_BLOB_SIZE / 2);
	for (int i = 0; i < blobSize; i++)
		if (blob[i] != _blob[i])
			return false;
	return true;
}

bool checkBlockBlob(const unsigned char *_blob) {
	for (int i = 0; i < NONCE_LOCATION; i++)
		if (blob[i] != _blob[i])
			return false;
	return true;
}

static void checkPoolResponds() {
	char msg[4096];
	int len = 0;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = CHECK_POOL_TIMEOUT*1000L;
	fd_set set;
	FD_ZERO(&set); 						// clear the set
	FD_SET(connections, &set); 	// add our file descriptor to the set

	int rv = select(connections + 1, &set, NULL, NULL, &tv);
	if (rv == -1) {
		debug("Mining pools failed to respond", NULL);
		return;
	} else if (rv == 0) {
		return;	// nothing to read
	} else {
		len = read(connections, msg, 4096);
		msg[len] = 0;
	}
	if (debugNetwork && current_index == 0 && len > 0)
		cout << START_YELLOW << "RECV " << msg << START_WHITE;

	checkNewBloc(msg);
	decodeHeight(msg);
	checkInvalidShare(msg);
}

uint32_t getVariant() {
	WaitForJob();
	char major_version = blob[0];

	if (major_version == 8) {
			return K12_ALGO;		// K12
	}
	else {
		exitOnError("unsupported protocol");
	}

	return 0;
}

static void submitResult(int64_t nonce, const unsigned char *result, int index) {
	if (connections == 0)
		return;	// connection lost

	// convert bin to hex
	char resultHex[65];
	for (int i = 0; i < 32; i++) {
		resultHex[i * 2] = CONVHEX[result[i] >> 4];
		resultHex[i * 2 + 1] = CONVHEX[result[i] & 0xf];
	}
	resultHex[64] = 0;

	unsigned char nonceHex[17];
	// K12_nonce64
	bin2hex((const unsigned char*) &nonce,8, nonceHex);
	nonceHex[16] = 0;

	char msg[4096];
	sprintf(msg, "{\"method\":\"submit\",\"params\":{\"id\":\"%s\",\"job_id\":\"%s\",\"nonce\":\"%s\",\"result\":\"%s\"},\"id\":1}\n", myIds[index], jobId, nonceHex, resultHex);

	if (debugNetwork && current_index == 0)
		cout << START_YELLOW << "SEND " << msg << START_WHITE;

	unsigned int sent = send(connections, msg, strlen(msg), MSG_NOSIGNAL);
	if (sent != strlen(msg)) {
		error("Connection lost", NULL);
		connections = 0;
		return ;
	}
	checkPoolResponds();
}

void notifyResult(int64_t nonce, const unsigned char *hash, unsigned char *_blob, uint32_t height) {
	sem_wait(&mutexQueue);
	msgResult[head].nonce = nonce;
	memcpy(msgResult[head].blob, _blob, MAX_BLOB_SIZE / 2);
	memcpy(msgResult[head].hash, hash, 64);
	head = (head + 1) % QUEUE_SIZE;
	sem_post(&mutexQueue);
}

static bool checkAndConsume() {
	while (tail != head) {
		// skip if target has been updated since GPU computed the hash
		if (!checkBlockBlob(msgResult[tail].blob)) {
			while (tail != head) {
				tail = (tail + 1) % QUEUE_SIZE;
				expiredShares++;
			}
		} else {
			submitResult(msgResult[tail].nonce, msgResult[tail].hash, current_index);
			tail = (tail + 1) % QUEUE_SIZE;
		}
	}
	return true;
}

void requestStop() {
	stopRequested = true;
}

bool getStopRequested() {
	return stopRequested;
}

uint32_t getHeight() {
	return height;
}

int getInvalidShares() {
	return invalidShares;
}

int getExpiredShares() {
	return expiredShares;
}

void setHashesPerSec(float _hashesPerSec) {
	hashesPerSec = _hashesPerSec;
}

float getHashesPerSec() {
	return hashesPerSec;
}

void setTotalShares(int _totalShares) {
	totalShares = _totalShares;
}

uint32_t getTotalShares() {
	return totalShares;
}

void *networkThread(void *args) {
	while (!stopRequested) {
		checkAndConsume();
		if (current_index == 0 && connections == 0) {
			error("Mining pool connection lost....", "will retry in 10 seconds");
			sleep(10);
			error("try to reconnect", "to mining pool");
			if (lookForPool())
				connectToPool();
		}
		checkPoolResponds();
	}
	return NULL;
}

void startNetworkBG() {
	pthread_create(&thread_id, NULL, networkThread, NULL);
}

void initNetwork(const CPUMiner &cpuMiner) {
	sem_init(&mutex, 0, 1);
	sem_init(&mutexQueue, 0, 1);

	tail = 0;
	head = 0;
	stopRequested = false;
	mpool = 0;
	srand(now());
	memset(hostname, 0, MAX_HOSTNAME_SIZE);
	memset(wallet, 0, MAX_WALLET_SIZE);
	memset(password, 0, MAX_WALLET_SIZE);
	current_index = 0;
	dpool = now() - TimeRotate * 0.01*(float)(rand()%100);
	debugNetwork = cpuMiner.debugNetwork;
}

void closeNetwork() {
	stopRequested = true;
}

int getCurrentPool() {
	return current_index;
}

int getCurrentIndex() {
	return current_index;
}
