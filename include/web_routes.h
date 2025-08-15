#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

#include <Arduino.h>

void setupWebRoutes();
void serveStaticFile(const String& path, const String& contentType);
String getUptime();
void addSecurityHeaders();
bool checkRateLimit();

// API Handler fonksiyonları
void handleStatusAPI();
void handleGetSettingsAPI();
void handlePostSettingsAPI();
void handleFaultRequest(bool isFirst);
void handleGetNtpAPI();
void handlePostNtpAPI();
void handleGetBaudRateAPI();
void handlePostBaudRateAPI();
void handleGetLogsAPI();
void handleClearLogsAPI();
void handleSystemInfoAPI();
void handleSessionRefresh();

#endif