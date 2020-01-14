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
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <time.h>
#include "log.hpp"

static enum LogLevel log_level = LEVEL_DEBUG;
using namespace std;

void exitOnError(const char *s) {
	cerr << s << "\n";
	exit(-1);
}

void debug(const char *title, const char *msg) {
	if (log_level <= LEVEL_DEBUG) {
		if (msg != NULL) cout << title << " " << msg << "\n";
		else cout << title << "\n";
	}
}

void debugnc(const char *title, const char *msg) {
	if (log_level <= LEVEL_DEBUG) {
		if (msg != NULL) cout << title << " " << msg;
		else cout << title ;
	}
}

void error(const char *title, const char *msg) {
	if (log_level <= LEVEL_ERROR) {
		if (msg != NULL) cout << title << " " << msg << "\n";
		else cout << title << "\n";
	}
}

void errornc(const char *title, const char *msg) {
	if (log_level <= LEVEL_ERROR) {
		if (msg != NULL) cout << title << " " << msg;
		else cout << title ;
	}
}

void printDate() {
	char timestr[256];
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	strftime(timestr, 256, "%FT%T", &tm);
	printf("[%s] ", timestr);
}
