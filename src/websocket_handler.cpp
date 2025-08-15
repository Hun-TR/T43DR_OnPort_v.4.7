#include "websocket_handler.h"
#include "log_system.h"
#include "settings.h"
#include "auth_system.h"
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// External functions
extern String getCurrentDateTime();
extern String getUptime();
extern bool isTimeSynced();

// WebSocket server instance
WebSocketsServer webSocket(WEBSOCKET_PORT);

// Client authentication tracking
struct WSClient {
    bool authenticated;
    unsigned long lastPing;
    String sessionId;
};

WSClient wsClients[5]; // Max 5 concurrent WebSocket connections

// WebSocket başlatma
void initWebSocket() {
    // WebSocket server'ı başlat
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    // Client array'i temizle
    for (int i = 0; i < 5; i++) {
        wsClients[i].authenticated = false;
        wsClients[i].lastPing = 0;
        wsClients[i].sessionId = "";
    }
    
    addLog("✅ WebSocket server başlatıldı (Port " + String(WEBSOCKET_PORT) + ")", SUCCESS, "WS");
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED: {
            wsClients[num].authenticated = false;
            wsClients[num].sessionId = "";
            addLog("WebSocket client #" + String(num) + " bağlantısı kesildi", INFO, "WS");
            break;
        }
        
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            addLog("WebSocket client #" + String(num) + " bağlandı: " + ip.toString(), INFO, "WS");
            
            // İlk bağlantıda authentication isteği gönder
            JsonDocument doc;  // Yeni ArduinoJson v7 syntax
            doc["type"] = "auth_required";
            doc["message"] = "Please authenticate";
            
            String output;
            serializeJson(doc, output);
            webSocket.sendTXT(num, output);
            break;
        }
        
        case WStype_TEXT: {
            // Gelen mesajı parse et
            JsonDocument doc;  // Yeni ArduinoJson v7 syntax
            DeserializationError error = deserializeJson(doc, payload, length);
            
            if (error) {
                addLog("WebSocket JSON parse hatası", ERROR, "WS");
                return;
            }
            
            String cmd = doc["cmd"] | "";
            
            // Authentication kontrolü
            if (cmd == "auth") {
                String token = doc["token"] | "";
                // Token kontrolü (basitleştirilmiş - gerçek uygulamada JWT kullanın)
                if (checkSession()) {
                    wsClients[num].authenticated = true;
                    wsClients[num].lastPing = millis();
                    
                    JsonDocument response;  // Yeni syntax
                    response["type"] = "auth_success";
                    response["message"] = "Authenticated successfully";
                    
                    String output;
                    serializeJson(response, output);
                    webSocket.sendTXT(num, output);
                    
                    // İlk durum bilgisini gönder
                    broadcastStatus();
                } else {
                    JsonDocument response;  // Yeni syntax
                    response["type"] = "auth_failed";
                    response["message"] = "Authentication failed";
                    
                    String output;
                    serializeJson(response, output);
                    webSocket.sendTXT(num, output);
                    
                    // Bağlantıyı kes
                    webSocket.disconnect(num);
                }
            }
            // Ping/Pong mekanizması
            else if (cmd == "ping") {
                if (wsClients[num].authenticated) {
                    wsClients[num].lastPing = millis();
                    
                    JsonDocument response;  // Yeni syntax
                    response["type"] = "pong";
                    response["timestamp"] = millis();
                    
                    String output;
                    serializeJson(response, output);
                    webSocket.sendTXT(num, output);
                }
            }
            // Durum isteği
            else if (cmd == "get_status") {
                if (wsClients[num].authenticated) {
                    broadcastStatus();
                }
            }
            // Log isteği
            else if (cmd == "get_logs") {
                if (wsClients[num].authenticated) {
                    // Son 10 logu gönder
                    for (int i = 0; i < min(10, totalLogs); i++) {
                        int idx = (logIndex - 1 - i + 50) % 50;
                        if (logs[idx].message.length() > 0) {
                            JsonDocument logDoc;  // Yeni syntax
                            logDoc["type"] = "log";
                            logDoc["timestamp"] = logs[idx].timestamp;
                            logDoc["message"] = logs[idx].message;
                            logDoc["level"] = logLevelToString(logs[idx].level);
                            logDoc["source"] = logs[idx].source;
                            
                            String output;
                            serializeJson(logDoc, output);
                            webSocket.sendTXT(num, output);
                        }
                    }
                }
            }
            break;
        }
        
        case WStype_BIN:
            // Binary veri işleme (kullanılmıyor)
            break;
            
        case WStype_ERROR:
            addLog("WebSocket hatası", ERROR, "WS");
            break;
            
        case WStype_PING:
            // Auto handled
            break;
            
        case WStype_PONG:
            // Auto handled
            break;
            
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            // Fragment handling (büyük mesajlar için)
            break;
    }
}

// WebSocket loop - main loop'ta çağrılacak
void handleWebSocket() {
    webSocket.loop();
    
    // Timeout kontrolü - 30 saniye inactive olan clientları kes
    unsigned long now = millis();
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated && wsClients[i].lastPing > 0) {
            if (now - wsClients[i].lastPing > 30000) {
                webSocket.disconnect(i);
                wsClients[i].authenticated = false;
                addLog("WebSocket client #" + String(i) + " timeout", WARN, "WS");
            }
        }
    }
}

// Log mesajı broadcast
void broadcastLog(const String& message, const String& level, const String& source) {
    JsonDocument doc;  // Yeni syntax
    doc["type"] = "log";
    doc["timestamp"] = getFormattedTimestamp();
    doc["message"] = message;
    doc["level"] = level;
    doc["source"] = source;
    
    String output;
    serializeJson(doc, output);
    
    // Tüm authenticated clientlara gönder
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated) {
            webSocket.sendTXT(i, output);
        }
    }
}

// Sistem durumu broadcast
void broadcastStatus() {
    JsonDocument doc;  // Yeni syntax
    doc["type"] = "status";
    doc["datetime"] = getCurrentDateTime();
    doc["uptime"] = getUptime();
    doc["deviceName"] = settings.deviceName;
    doc["tmName"] = settings.transformerStation;
    doc["deviceIP"] = settings.local_IP.toString();
    doc["baudRate"] = settings.currentBaudRate;
    doc["ethernetStatus"] = ETH.linkUp();
    doc["timeSynced"] = isTimeSynced();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wsClients"] = getWebSocketClientCount();
    
    String output;
    serializeJson(doc, output);
    
    // Tüm authenticated clientlara gönder
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated) {
            webSocket.sendTXT(i, output);
        }
    }
}

// Arıza verisi broadcast
void broadcastFault(const String& faultData) {
    JsonDocument doc;  // Yeni syntax
    doc["type"] = "fault";
    doc["timestamp"] = getFormattedTimestamp();
    doc["data"] = faultData;
    
    String output;
    serializeJson(doc, output);
    
    // Tüm authenticated clientlara gönder
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated) {
            webSocket.sendTXT(i, output);
        }
    }
}

// Belirli bir cliente mesaj gönder
void sendToClient(uint8_t clientNum, const String& message) {
    if (clientNum < 5 && wsClients[clientNum].authenticated) {
        String msg = message;  // const'ı kaldırmak için kopya oluştur
        webSocket.sendTXT(clientNum, msg);
    }
}

// Tüm clientlara mesaj gönder
void sendToAllClients(const String& message) {
    String msg = message;  // const'ı kaldırmak için kopya oluştur
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated) {
            webSocket.sendTXT(i, msg);
        }
    }
}

// WebSocket bağlantı durumu
bool isWebSocketConnected() {
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated) {
            return true;
        }
    }
    return false;
}

// Bağlı client sayısı
int getWebSocketClientCount() {
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (wsClients[i].authenticated) {
            count++;
        }
    }
    return count;
}