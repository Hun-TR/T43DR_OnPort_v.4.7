#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <Arduino.h>
#include <ETH.h>

struct NetworkConfig {
    bool useDHCP;
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
};

extern NetworkConfig netConfig;

void loadNetworkConfig();
void saveNetworkConfig(bool useDHCP, String ip, String gw, String sn, String d1, String d2);
void initEthernetAdvanced();
String getNetworkConfigJSON();

#endif // NETWORK_CONFIG_Hpio run --t