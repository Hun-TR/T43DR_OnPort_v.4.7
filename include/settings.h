#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <WebServer.h>
#include <ETH.h>

struct Settings {
    IPAddress local_IP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress primaryDNS;
    String deviceName;
    String transformerStation;
    String username;
    String passwordSalt;
    String passwordHash;
    long currentBaudRate;
    bool isLoggedIn;
    unsigned long sessionStartTime;
    unsigned long SESSION_TIMEOUT;
};

extern WebServer server;
extern Settings settings;

void loadSettings();
bool saveSettings(const String& newDevName, const String& newTmName, const String& newUsername, const String& newPassword);
void initEthernet();

#endif