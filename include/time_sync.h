#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>

// Fonksiyon tanımlamaları
bool requestTimeFromDsPIC();
bool parseTimeResponse(const String& response);
String formatDate(const String& dateStr);
String formatTime(const String& timeStr);
void updateSystemTime();
void checkTimeSync();
String getCurrentDateTime();
String getCurrentDate();
String getCurrentTime();
bool isTimeSynced();
String getTimeSyncStats();

#endif // TIME_SYNC_H