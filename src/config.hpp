#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#define VERSION 		"0.0"

#define MAXSIZE			(1<<8)
#define MAX_ADRESS_SIZE		MAXSIZE
#define MAX_POOLNAME_SIZE	MAXSIZE
#define MAX_PASSWORD_SIZE	MAXSIZE
#define CONFIG_FILENAME		"config.json"
#define MAX_CPUS		(1<<9)
#define DEFAULT_CONSOLE_REFRESH_RATE	10
#define K12_ALGO		0x100

typedef struct Config {
	char address[MAX_ADRESS_SIZE];
	char poolAddress[MAX_POOLNAME_SIZE];
	char poolPassword[MAX_PASSWORD_SIZE];
	int  poolPort;
	int   nbCPUs;
	bool  debugNetwork;
	int   consoleRefreshRate;
} Config;

typedef struct CPUMiner {
	bool 	debugNetwork;
	int 	index;
} CPUMiner;

void makeConfig();
bool readConfig();
bool checkConfig();
extern Config config;

#endif /* CONFIG_HPP_ */
