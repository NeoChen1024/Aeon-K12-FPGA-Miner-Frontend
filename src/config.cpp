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
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>


#include "log.hpp"
#include "config.hpp"
#include "miner.hpp"
#include "network.hpp"

using namespace std;

Config config;

static const char* getJSONEntryLocation(const char *s, int len, const char *needle, bool isFatal)  {
	const char *loc = strstr(s, needle);
	if (loc == NULL && isFatal) {
		error("Mandatory configuration property not found:",needle);
		exitOnError("Fix config.json file");
	}
	if (loc == NULL)
		return nullptr;

	loc ++;	// skip "
	int i = loc - s;
	while (i < len && (*loc) != '"') {
		i++;
		loc ++;
	}
	if (i == len)
		return nullptr;
	return loc;
}

static void fillStringProperty(char *property, int len, const char*src) {
	while (*src != 0 && *src != '"') src++;
	if (*src == 0) return;
	src++;
	while (*src != 0 && *src != '"') src++;
	if (*src == 0) return;
	src++;
	int i = 0;
	while (i < len-1 && src[i] != '"') {
		property[i] = src[i];
		i++;
	}
	property[i] = 0;
}

static int getIntProperty(const char*src) {
	while (*src != 0 && *src != ':') src++;
	if (*src == 0) return 0;
	src++;	// skip :

	while (*src != 0 && (*src < '0' || *src > '9')) src++;
	if (*src == 0) return 0;

	char tmp[16];
	int i = 0;
	while (*src >= '0' && *src <= '9' && i < 16) {
		tmp[i] = *src;
		src++;
		i++;
	}
	tmp[i] = 0;
	return atoi(tmp);
}

static string createJsonFromConfig() {
	ostringstream s;
	s << "{\n";
	s << "\t\"pool_address\" : \"" << config.poolAddress << ":" << config.poolPort<< "\",\n";
	s << "\t\"wallet_address\" : \"" << config.address << "\",\n";
	s << "\t\"pool_password\" : \"" << config.poolPassword << "\",\n";
	s << "\t\"debug_network\" : \"" << (config.debugNetwork? "true" : "false") << "\",\n";
	s << "\t\"console_refresh_rate\" : \"" << config.consoleRefreshRate  << "\",\n";
	s << "\t\"number_cpus\" : \"" << config.nbCPUs  << "\"\n";
	s << "}\n";
	return s.str();
}

static void decodeConfig(const char *conf)  {
	const char *loc;
	int len = strlen(conf);
	char tmp[256];

	// decode wallet
	loc = getJSONEntryLocation(conf,len,"pool_address",true);
	fillStringProperty(tmp,256,loc);
	int i =0;
	while (tmp[i] != ':' && tmp[i] != 0 && i < MAX_POOLNAME_SIZE) {
		config.poolAddress[i] = tmp[i];
		i ++;
	}
	config.poolAddress[i] = 0;
	i++;
	config.poolPort = atoi(tmp+i);

	// decode wallet
	loc = getJSONEntryLocation(conf,len,"wallet_address",true);
	fillStringProperty(config.address,MAX_ADRESS_SIZE,loc);

	// decode password - can be empty but must be defined
	loc = getJSONEntryLocation(conf,len,"pool_password",true);
	fillStringProperty(config.poolPassword,MAX_PASSWORD_SIZE,loc);

	loc = getJSONEntryLocation(conf,len,"debug_network",false);
	if (loc != nullptr) {
		fillStringProperty(tmp,128,loc);
		config.debugNetwork = strcmp(tmp,"true") == 0;
	} else
		config.debugNetwork = false;

	loc = getJSONEntryLocation(conf,len,"console_refresh_rate",false);
	if (loc != nullptr) {
		fillStringProperty(tmp,128,loc);
		config.consoleRefreshRate = atoi(tmp);
	} else
		config.consoleRefreshRate = DEFAULT_CONSOLE_REFRESH_RATE;

	loc = getJSONEntryLocation(conf,len,"wallet_address",true);
	fillStringProperty(config.address,MAX_ADRESS_SIZE,loc);

	loc = getJSONEntryLocation(conf,len,"number_cpus",false);
	if (loc != nullptr)
		config.nbCPUs = getIntProperty(loc);
	else
		config.nbCPUs = 1;
}

static void setTerminalBehavior() {
	int oldfl = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL,  oldfl & ~O_NONBLOCK);
}


static string filterComments(const string & s) {
	string ret;

	for (uint32_t i=0; i< s.size(); i++) {
		if (s.at(i) == '/' && s.at(i+1) == '/') {
			while (s.at(i) != '\n' && i < s.size())
				i++;
		}
		if (s.at(i) == '/' && s.at(i+1) == '*') {
			i += 2;
			while (i < s.size() && !(s.at(i) == '*'  && s.at(i+1) == '/' )) {
				i++;
			}
			i += 2;
		}
		ret.push_back(s.at(i));
	}
	return ret;
}

void makeConfig() {
	std::string input;
	setTerminalBehavior();
	cout << "\nNo config.json file found, entering configuration setup...\n";

	cout<<"    _                      \n";
	cout<<"   / \\   ___  ___  _ __    \n";
	cout<<"  / _ \\ / _ \\/ _ \\| '_ \\   \n";
	cout<<" / ___ \\  __/ (_) | | | |  \n";
	cout<<"/_/   \\_\\___|\\___/|_| |_|  \n";
	cout << "\n";

pool_select:
	cout << "Mining pool address (hostname/IP): ";
	getline(cin, input );
	if ( input.empty() ) goto pool_select;
	else {
		istringstream stream( input );
		stream >> config.poolAddress;
	}

port_select:
	cout << "Mining pool port: ";
	getline(cin, input );
	if ( input.empty() ) goto port_select;
	else {
		istringstream stream( input );
		stream >> config.poolPort;
	}

	registerPool(config.poolAddress,config.poolPort,"","");
	if (!lookForPool()) {
		cout << "Can't connect to the pool at " << config.poolAddress << ":" << config.poolPort << "\n";
		goto pool_select;
	}
	closeConnection();

address_select:
	cout << "Your address (with optional .something at the end): ";
	getline(cin, input );
	if ( input.empty() ) goto address_select;
	else {
		istringstream stream( input );
		stream >> config.address;
	}
	cout << "Password (or x if none): ";
	getline(cin, input );
	if ( !input.empty() ) {
		istringstream stream( input );
		stream >> config.poolPassword;
	} else {
		config.poolPassword[0] = 'x';
		config.poolPassword[1] = 0;
	}

	cout << "Console refresh rate (default 30s): ";
	getline(cin, input);
	if ( input.empty() ) {
	config.consoleRefreshRate = DEFAULT_CONSOLE_REFRESH_RATE;
	} else {
			istringstream stream(input);
		stream >> config.consoleRefreshRate;
	}

	cout << "CPUs: ";
	getline(cin, input);
	if ( input.empty() ) {
	config.nbCPUs = 1;
	} else {
		istringstream stream(input);
		stream >> config.nbCPUs;
	}

	string confStr = createJsonFromConfig();
	ofstream out("config.json");
	out << confStr;
	out.close();

	cout << "Proposed configuration:\n";
	puts(confStr.c_str());
	cout << "Happy FPGA hashing,\n";
	cout << " _____        _               _  \n";
	cout << "| ____|_ __  (_) ___  _   _  | | \n";
	cout << "|  _| | '_ \\ | |/ _ \\| | | | | | \n";
	cout << "| |___| | | || | (_) | |_| | |_| \n";
	cout << "|_____|_| |_|/ |\\___/ \\__, | (_) \n";
	cout << "           |__/       |___/      \n\n";
}

bool readConfig() {
	ifstream in(CONFIG_FILENAME);
	string confStr((istreambuf_iterator<char>(in)),istreambuf_iterator<char>());
	in.close();
	confStr = filterComments(confStr);

	decodeConfig(confStr.c_str());
	return true;
}

bool checkConfig() {
	ifstream in(CONFIG_FILENAME);
	string confStr((istreambuf_iterator<char>(in)),istreambuf_iterator<char>());
	in.close();
	return confStr.size() > 0;
}
