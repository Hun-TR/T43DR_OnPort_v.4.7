#ifndef LOG_SYSTEM_H
#define LOG_SYSTEM_H

#include <Arduino.h>

enum LogLevel {
    ERROR = 0,
    WARN = 1,
    INFO = 2,
    DEBUG = 3,
    SUCCESS = 4
};

struct LogEntry {
    String timestamp;
    String message;
    LogLevel level;
    String source;
    unsigned long millis_time;
};

extern LogEntry logs[50];
extern int logIndex;
extern int totalLogs;

void initLogSystem();
void addLog(const String& msg, LogLevel level, const String& source);
String logLevelToString(LogLevel level);
void clearLogs();
String getFormattedTimestamp();
String getFormattedTimestampFallback();

#endif