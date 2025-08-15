#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// WebSocket port numarası
#define WEBSOCKET_PORT 81

// WebSocket event türleri
enum WSEventType {
    WS_EVENT_LOG,
    WS_EVENT_STATUS,
    WS_EVENT_FAULT,
    WS_EVENT_CONFIG,
    WS_EVENT_UART
};

// WebSocket handler fonksiyonları
void initWebSocket();
void handleWebSocket();
void broadcastLog(const String& message, const String& level, const String& source);
void broadcastStatus();
void broadcastFault(const String& faultData);
void sendToClient(uint8_t clientNum, const String& message);
void sendToAllClients(const String& message);
bool isWebSocketConnected();
int getWebSocketClientCount();

// WebSocket event callback
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// Internal helper functions - sadece cpp dosyasında kullanılacak
void sendInitialDataToClient(uint8_t clientNum);
void sendStatusToClient(uint8_t clientNum);
void sendLogsToClient(uint8_t clientNum);

#endif // WEBSOCKET_HANDLER_H