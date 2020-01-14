#ifndef LOG_HPP_
#define LOG_HPP_

enum LogLevel {
	LEVEL_DEBUG,
	LEVEL_WARN,
	LEVEL_ERROR
};

void exitOnError(const char *s);
void debug(const char *title,const char *msg);
void debugnc(const char *title,const char *msg);
void error(const char *title,const char *msg);
void errornc(const char *title,const char *msg);
void printDate();

#endif /* LOG_HPP_ */
