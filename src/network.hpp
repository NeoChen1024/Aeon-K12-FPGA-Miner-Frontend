#ifndef NETWORK_HPP_
#define NETWORK_HPP_

#define CONNECT_TIMEOUT 3
#define CHECK_POOL_TIMEOUT 100L
#define MINING_AGENT	"Aeon K12 FPGA Miner"
#define MAX_BLOB_SIZE 	256
#define MAX_TARGET_SIZE 17

void initNetwork(const CPUMiner &cpuMiner);
void closeNetwork();
bool lookForPool();
bool connectToPool();
void closeConnection();
void getCurrentBlob(unsigned char *blob, int *size);
uint64_t getTarget();
uint32_t getHeight();
void applyNonce(unsigned char *input, uint64_t nonce);
void notifyResult(int64_t nonce , const unsigned char *hash, unsigned char *blob, uint32_t height);
void startNetworkBG();
bool checkBlob(const unsigned char *blob);
bool getStopRequested();
void requestStop();
uint64_t getRandomNonce(int gpuIndex);
uint32_t getVariant();
int getInvalidShares();
int getExpiredShares();
int getCurrentPool();
int getCurrentIndex();
void setHashesPerSec(float hashesPerSec);
float getHashesPerSec();
void setTotalShares(int totalShares);
uint32_t getTotalShares();
void registerPool(const char* name, int port,const char *wallet, const char *password);

#endif /* NETWORK_HPP_ */
