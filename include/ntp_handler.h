#ifndef NTP_HANDLER_H
#define NTP_HANDLER_H

#include <Arduino.h>

struct ReceivedTimeData {
    String date;
    String time;
    bool isValid;
    unsigned long lastUpdate;
};

struct NTPConfig {
    char ntpServer1[64];
    char ntpServer2[64];
    int timezone;
    bool enabled;
};

extern ReceivedTimeData receivedTime;
extern NTPConfig ntpConfig;
extern bool ntpConfigured;

void initNTPHandler();
void processReceivedData();
bool loadNTPSettings();
bool saveNTPSettings(const String& server1, const String& server2, int timezone);
void sendNTPConfigToBackend();
String formatDate(const String& dateStr);
String formatTime(const String& timeStr);
void parseTimeData(const String& data);
void readBackendData();
bool isTimeDataValid();
String getCurrentDateTime();
String getCurrentDate();
String getCurrentTime();
bool isValidIPOrDomain(const String& address);
bool isNTPSynced();
void resetNTPSettings();

#endif