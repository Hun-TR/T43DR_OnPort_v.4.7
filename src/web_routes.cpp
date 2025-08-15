#include "web_routes.h"
#include "auth_system.h"
#include "settings.h"
#include "ntp_handler.h"
#include "uart_handler.h"
#include "log_system.h"
#include "backup_restore.h"      // Yeni eklenen
#include "password_policy.h"     // Yeni eklenen
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// External fonksiyonlar - time_sync.cpp'den
extern String getCurrentDateTime();
extern String getCurrentDate();
extern String getCurrentTime();
extern bool isTimeSynced();

extern WebServer server;
extern Settings settings;
extern bool ntpConfigured;

// Cache için statik değişkenler
static String cachedIndexHtml = "";
static String cachedStyleCss = "";
static String cachedScriptJs = "";
static bool filesLoaded = false;

// Dosyaları belleğe yükle (bir kez)
void loadFilesToMemory() {
    if (filesLoaded) return;
    
    // HTML dosyalarını yükle
    File file = LittleFS.open("/index.html", "r");
    if (file) {
        cachedIndexHtml = file.readString();
        file.close();
    }
    
    file = LittleFS.open("/style.css", "r");
    if (file) {
        cachedStyleCss = file.readString();
        file.close();
    }
    
    file = LittleFS.open("/script.js", "r");
    if (file) {
        cachedScriptJs = file.readString();
        file.close();
    }
    
    filesLoaded = true;
}

// Hızlı statik dosya servisi
void serveCachedFile(const String& filename, const String& contentType) {
    // Cache'ten sun
    if (filename == "/index.html" && cachedIndexHtml.length() > 0) {
        server.send(200, contentType, cachedIndexHtml);
        return;
    }
    if (filename == "/style.css" && cachedStyleCss.length() > 0) {
        server.send(200, contentType, cachedStyleCss);
        return;
    }
    if (filename == "/script.js" && cachedScriptJs.length() > 0) {
        server.send(200, contentType, cachedScriptJs);
        return;
    }
    
    // Cache'te yoksa dosyadan oku
    if (!LittleFS.exists(filename)) {
        server.send(404, "text/plain", "404: Not Found");
        return;
    }
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        server.send(500, "text/plain", "500: File Error");
        return;
    }
    
    // Chunked transfer kullan (daha hızlı)
    server.streamFile(file, contentType);
    file.close();
}

String getUptime() {
    unsigned long sec = millis() / 1000;
    char buffer[32];
    sprintf(buffer, "%lu:%02lu:%02lu", sec/3600, (sec%3600)/60, sec%60);
    return String(buffer);
}

// API Handler'lar - Optimize edildi

void handleStatusAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    // Static JSON buffer kullan (heap yerine stack)
    char json[512];
    snprintf(json, sizeof(json),
        "{\"datetime\":\"%s\","
        "\"uptime\":\"%s\","
        "\"deviceName\":\"%s\","
        "\"tmName\":\"%s\","
        "\"deviceIP\":\"%s\","
        "\"baudRate\":%ld,"
        "\"ethernetStatus\":\"%s\","
        "\"ntpConfigStatus\":\"%s\","
        "\"backendStatus\":\"%s\"}",
        getCurrentDateTime().c_str(),
        getUptime().c_str(),
        settings.deviceName.c_str(),
        settings.transformerStation.c_str(),
        settings.local_IP.toString().c_str(),
        settings.currentBaudRate,
        ETH.linkUp() ? "Bağlı" : "Yok",
        ntpConfigured ? "Aktif" : "Pasif",
        isTimeSynced() ? "Aktif" : "Pasif"
    );
    
    server.send(200, "application/json", json);
}

void handleGetSettingsAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    char json[256];
    snprintf(json, sizeof(json),
        "{\"deviceName\":\"%s\",\"tmName\":\"%s\",\"username\":\"%s\"}",
        settings.deviceName.c_str(),
        settings.transformerStation.c_str(),
        settings.username.c_str()
    );
    
    server.send(200, "application/json", json);
}

void handlePostSettingsAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (!saveSettings(
        server.arg("deviceName"),
        server.arg("tmName"),
        server.arg("username"),
        server.arg("password")
    )) {
        server.send(400, "text/plain", "Error");
        return;
    }
    
    server.send(200, "text/plain", "OK");
}

void handleFaultRequest(bool isFirst) {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    bool success = isFirst ? requestFirstFault() : requestNextFault();
    
    if (success) {
        String response = getLastFaultResponse();
        server.send(200, "text/plain", response);
    } else {
        server.send(500, "text/plain", "Error");
    }
}

void handleGetNtpAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    char json[256];
    snprintf(json, sizeof(json),
        "{\"ntpServer1\":\"%s\",\"ntpServer2\":\"%s\",\"timezone\":%d}",
        ntpConfig.ntpServer1,
        ntpConfig.ntpServer2,
        ntpConfig.timezone
    );
    
    server.send(200, "application/json", json);
}

void handlePostNtpAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (!saveNTPSettings(
        server.arg("ntpServer1"),
        server.arg("ntpServer2"),
        server.arg("timezone").toInt()
    )) {
        server.send(400, "text/plain", "Error");
        return;
    }
    
    sendNTPConfigToBackend();
    server.send(200, "text/plain", "OK");
}

void handleGetBaudRateAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    char json[64];
    snprintf(json, sizeof(json), "{\"baudRate\":%ld}", settings.currentBaudRate);
    server.send(200, "application/json", json);
}

void handlePostBaudRateAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    long newBaud = server.arg("baud").toInt();
    
    if (!changeBaudRate(newBaud)) {
        server.send(500, "text/plain", "Error");
        return;
    }
    
    server.send(200, "text/plain", "OK");
}

void handleGetLogsAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    String json = "[";
    int count = min(15, totalLogs);
    
    for (int i = 0; i < count; i++) {
        int idx = (logIndex - 1 - i + 50) % 50;
        if (logs[idx].message.length() > 0) {
            if (i > 0) json += ",";
            json += "{\"t\":\"" + logs[idx].timestamp + "\",";
            json += "\"m\":\"" + logs[idx].message + "\",";
            json += "\"l\":\"" + logLevelToString(logs[idx].level) + "\",";
            json += "\"s\":\"" + logs[idx].source + "\"}";
        }
    }
    json += "]";
    
    server.send(200, "application/json", json);
}

void handleClearLogsAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    clearLogs();
    server.send(200, "text/plain", "OK");
}

// UART Test API Handler
void handleUARTTestAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    // UART test fonksiyonu
    bool result = testUARTConnection();
    if (result) {
        server.send(200, "application/json", "{\"success\":true,\"message\":\"UART connection successful\"}");
    } else {
        server.send(500, "application/json", "{\"success\":false,\"message\":\"UART connection failed\"}");
    }
}

// Web rotaları
void setupWebRoutes() {
    // Dosyaları belleğe yükle
    loadFilesToMemory();
    
    // Ana sayfa
    server.on("/", HTTP_GET, []() {
        if (!checkSession()) { 
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        serveCachedFile("/index.html", "text/html");
    });
    
    // Login
    server.on("/login", HTTP_GET, []() {
        if (checkSession()) {
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "Already logged in");
            return;
        }
        serveCachedFile("/login.html", "text/html");
    });
    
    // Statik dosyalar - Cache'ten
    server.on("/style.css", HTTP_GET, []() {
        serveCachedFile("/style.css", "text/css");
    });
    
    server.on("/script.js", HTTP_GET, []() {
        serveCachedFile("/script.js", "application/javascript");
    });
    
    // Diğer sayfalar
    server.on("/account", HTTP_GET, []() {
        if (!checkSession()) {
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        serveCachedFile("/account.html", "text/html");
    });
    
    server.on("/fault", HTTP_GET, []() {
        if (!checkSession()) {
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        serveCachedFile("/fault.html", "text/html");
    });
    
    server.on("/ntp", HTTP_GET, []() {
        if (!checkSession()) {
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        serveCachedFile("/ntp.html", "text/html");
    });
    
    server.on("/baudrate", HTTP_GET, []() {
        if (!checkSession()) {
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        serveCachedFile("/baudrate.html", "text/html");
    });
    
    server.on("/log", HTTP_GET, []() {
        if (!checkSession()) {
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        serveCachedFile("/log.html", "text/html");
    });
    
    // Parola değiştirme sayfası
    server.on("/change-password", HTTP_GET, []() {
        if (!checkSession()) {
            server.sendHeader("Location", "/login");
            server.send(302, "text/plain", "Login required");
            return;
        }
        if (mustChangePassword()) {
            handlePasswordChangePage();
        } else {
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "Password change not required");
        }
    });
    
    // Auth endpoints
    server.on("/login", HTTP_POST, handleUserLogin);
    server.on("/logout", HTTP_GET, handleUserLogout);
    
    // API endpoints
    server.on("/api/status", HTTP_GET, handleStatusAPI);
    server.on("/api/settings", HTTP_GET, handleGetSettingsAPI);
    server.on("/api/settings", HTTP_POST, handlePostSettingsAPI);
    server.on("/api/faults/first", HTTP_POST, []() { handleFaultRequest(true); });
    server.on("/api/faults/next", HTTP_POST, []() { handleFaultRequest(false); });
    server.on("/api/faults/refresh", HTTP_POST, []() { handleFaultRequest(false); });
    server.on("/api/ntp", HTTP_GET, handleGetNtpAPI);
    server.on("/api/ntp", HTTP_POST, handlePostNtpAPI);
    server.on("/api/baudrate", HTTP_GET, handleGetBaudRateAPI);
    server.on("/api/baudrate", HTTP_POST, handlePostBaudRateAPI);
    server.on("/api/logs", HTTP_GET, handleGetLogsAPI);
    server.on("/api/logs/clear", HTTP_POST, handleClearLogsAPI);
    
    // Yeni API endpoints
    server.on("/api/backup/download", HTTP_GET, handleBackupDownload);
    server.on("/api/backup/upload", HTTP_POST, handleBackupUpload);
    server.on("/api/change-password", HTTP_POST, handlePasswordChangeAPI);
    server.on("/api/uart/test", HTTP_POST, handleUARTTestAPI);
    
    // 404
    server.onNotFound([]() {
        server.send(404, "text/plain", "404: Not Found");
    });
    
    // Server optimizasyonları
    server.enableCORS(false);  // CORS kapat
    server.enableDelay(false); // Delay kapat
    
    server.begin();
    
    addLog("✅ Web sunucu başlatıldı", SUCCESS, "WEB");
}