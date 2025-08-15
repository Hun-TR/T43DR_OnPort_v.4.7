#include "backup_restore.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include "settings.h"
#include "log_system.h"
#include "ntp_handler.h"
#include "crypto_utils.h"
#include "auth_system.h"  // checkSession için
#include <WebServer.h>

extern WebServer server;

// Ayarları JSON formatında export et
String exportSettingsToJSON() {
    // JSON document oluştur (2KB buffer) - Yeni ArduinoJson v7 syntax
    JsonDocument doc;
    
    // Versiyon bilgisi
    doc["version"] = "1.0";
    doc["timestamp"] = getFormattedTimestamp();
    doc["deviceId"] = ETH.macAddress();
    
    // Network ayarları
    JsonObject network = doc["network"].to<JsonObject>();
    network["localIP"] = settings.local_IP.toString();
    network["gateway"] = settings.gateway.toString();
    network["subnet"] = settings.subnet.toString();
    network["dns"] = settings.primaryDNS.toString();
    
    // Cihaz bilgileri
    JsonObject device = doc["device"].to<JsonObject>();
    device["name"] = settings.deviceName;
    device["tmName"] = settings.transformerStation;
    device["baudRate"] = settings.currentBaudRate;
    
    // Kullanıcı ayarları (şifre hariç)
    JsonObject user = doc["user"].to<JsonObject>();
    user["username"] = settings.username;
    user["sessionTimeout"] = settings.SESSION_TIMEOUT;
    
    // NTP ayarları
    JsonObject ntp = doc["ntp"].to<JsonObject>();
    ntp["server1"] = String(ntpConfig.ntpServer1);
    ntp["server2"] = String(ntpConfig.ntpServer2);
    ntp["timezone"] = ntpConfig.timezone;
    ntp["enabled"] = ntpConfig.enabled;
    
    // Log ayarları
    JsonObject logging = doc["logging"].to<JsonObject>();
    logging["maxLogs"] = 50;
    logging["currentLogs"] = totalLogs;
    
    // Sistem bilgileri
    JsonObject system = doc["system"].to<JsonObject>();
    system["freeHeap"] = ESP.getFreeHeap();
    system["chipRevision"] = ESP.getChipRevision();
    system["sdkVersion"] = ESP.getSdkVersion();
    system["flashSize"] = ESP.getFlashChipSize();
    
    // JSON'u string'e serialize et
    String output;
    serializeJsonPretty(doc, output);
    
    addLog("✅ Ayarlar JSON formatında export edildi", SUCCESS, "BACKUP");
    return output;
}

// JSON'dan ayarları import et
bool importSettingsFromJSON(const String& jsonData) {
    // JSON parse et - Yeni ArduinoJson v7 syntax
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
        addLog("❌ JSON parse hatası: " + String(error.c_str()), ERROR, "RESTORE");
        return false;
    }
    
    // Versiyon kontrolü
    String version = doc["version"] | "unknown";
    if (version != "1.0") {
        addLog("⚠️ Uyumsuz backup versiyonu: " + version, WARN, "RESTORE");
    }
    
    // Preferences'ı aç
    Preferences prefs;
    prefs.begin("app-settings", false);
    
    try {
        // Network ayarlarını yükle
        if (doc["network"].is<JsonObject>()) {
            JsonObject network = doc["network"];
            
            String ipStr = network["localIP"] | "192.168.1.160";
            String gwStr = network["gateway"] | "192.168.1.1";
            String snStr = network["subnet"] | "255.255.255.0";
            String dnsStr = network["dns"] | "8.8.8.8";
            
            // IP validasyonu ve kaydetme
            IPAddress testIP;
            if (testIP.fromString(ipStr)) {
                prefs.putString("local_ip", ipStr);
                settings.local_IP.fromString(ipStr);
            }
            if (testIP.fromString(gwStr)) {
                prefs.putString("gateway", gwStr);
                settings.gateway.fromString(gwStr);
            }
            if (testIP.fromString(snStr)) {
                prefs.putString("subnet", snStr);
                settings.subnet.fromString(snStr);
            }
            if (testIP.fromString(dnsStr)) {
                prefs.putString("dns", dnsStr);
                settings.primaryDNS.fromString(dnsStr);
            }
        }
        
        // Cihaz bilgilerini yükle
        if (doc["device"].is<JsonObject>()) {
            JsonObject device = doc["device"];
            
            String devName = device["name"] | "TEİAŞ EKLİM";
            String tmName = device["tmName"] | "Belirtilmemiş";
            long baudRate = device["baudRate"] | 115200;
            
            prefs.putString("dev_name", devName);
            prefs.putString("tm_name", tmName);
            prefs.putLong("baudrate", baudRate);
            
            settings.deviceName = devName;
            settings.transformerStation = tmName;
            settings.currentBaudRate = baudRate;
        }
        
        // Kullanıcı ayarlarını yükle (şifre hariç)
        if (doc["user"].is<JsonObject>()) {
            JsonObject user = doc["user"];
            
            String username = user["username"] | "admin";
            unsigned long timeout = user["sessionTimeout"] | 1800000;
            
            prefs.putString("username", username);
            settings.username = username;
            settings.SESSION_TIMEOUT = timeout;
        }
        
        // NTP ayarlarını yükle
        if (doc["ntp"].is<JsonObject>()) {
            JsonObject ntp = doc["ntp"];
            
            String server1 = ntp["server1"] | "pool.ntp.org";
            String server2 = ntp["server2"] | "time.google.com";
            int timezone = ntp["timezone"] | 3;
            bool enabled = ntp["enabled"] | true;
            
            // NTP ayarlarını kaydet
            Preferences ntpPrefs;
            ntpPrefs.begin("ntp-config", false);
            ntpPrefs.putString("ntp_server1", server1);
            ntpPrefs.putString("ntp_server2", server2);
            ntpPrefs.putInt("timezone", timezone);
            ntpPrefs.putBool("enabled", enabled);
            ntpPrefs.end();
            
            // Global NTP config'i güncelle
            server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
            server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
            ntpConfig.timezone = timezone;
            ntpConfig.enabled = enabled;
        }
        
        prefs.end();
        
        addLog("✅ Ayarlar başarıyla import edildi", SUCCESS, "RESTORE");
        addLog("⚠️ Yeniden başlatma gerekli", WARN, "RESTORE");
        
        return true;
        
    } catch (...) {
        prefs.end();
        addLog("❌ Ayarlar import edilemedi", ERROR, "RESTORE");
        return false;
    }
}

// Backup dosyasını kaydet
bool saveBackupToFile(const String& filename) {
    // JSON backup oluştur
    String jsonBackup = exportSettingsToJSON();
    
    // Dosyaya yaz
    File file = LittleFS.open("/" + filename, "w");
    if (!file) {
        addLog("❌ Backup dosyası oluşturulamadı: " + filename, ERROR, "BACKUP");
        return false;
    }
    
    file.print(jsonBackup);
    file.close();
    
    addLog("✅ Backup dosyası kaydedildi: " + filename, SUCCESS, "BACKUP");
    return true;
}

// Backup dosyasından yükle
bool loadBackupFromFile(const String& filename) {
    // Dosyayı aç
    File file = LittleFS.open("/" + filename, "r");
    if (!file) {
        addLog("❌ Backup dosyası bulunamadı: " + filename, ERROR, "RESTORE");
        return false;
    }
    
    // JSON'u oku
    String jsonData = file.readString();
    file.close();
    
    // Import et
    return importSettingsFromJSON(jsonData);
}

// Web API handler - Backup indir
void handleBackupDownload() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    // JSON backup oluştur
    String jsonBackup = exportSettingsToJSON();
    
    // Dosya adı oluştur
    String filename = "teias_backup_" + String(millis()) + ".json";
    
    // HTTP response headers
    server.sendHeader("Content-Type", "application/json");
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.sendHeader("Content-Length", String(jsonBackup.length()));
    
    // JSON'u gönder
    server.send(200, "application/json", jsonBackup);
    
    addLog("📥 Backup indirildi", INFO, "BACKUP");
}

// Web API handler - Backup yükle
void handleBackupUpload() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    HTTPUpload& upload = server.upload();
    static String uploadedData = "";
    
    if (upload.status == UPLOAD_FILE_START) {
        uploadedData = "";
        addLog("📤 Backup yükleme başladı: " + upload.filename, INFO, "RESTORE");
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Veriyi biriktir
        for (size_t i = 0; i < upload.currentSize; i++) {
            uploadedData += (char)upload.buf[i];
        }
        
    } else if (upload.status == UPLOAD_FILE_END) {
        // Import işlemini başlat
        if (importSettingsFromJSON(uploadedData)) {
            server.send(200, "text/plain", "Backup successfully restored. Device will restart.");
            
            // 2 saniye sonra restart
            delay(2000);
            ESP.restart();
        } else {
            server.send(400, "text/plain", "Backup restore failed");
        }
        
        uploadedData = "";
    }
}

// Otomatik backup oluştur (her gün)
void createAutomaticBackup() {
    static unsigned long lastBackup = 0;
    const unsigned long BACKUP_INTERVAL = 86400000; // 24 saat
    
    if (millis() - lastBackup > BACKUP_INTERVAL) {
        // Tarih damgalı dosya adı
        String filename = "auto_backup_" + getCurrentDate() + ".json";
        filename.replace(".", "_");
        filename.replace(" ", "_");
        filename.replace(":", "_");
        
        // Eski backupları temizle (max 7 adet)
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        int backupCount = 0;
        String oldestBackup = "";
        
        while (file) {
            String fname = file.name();
            if (fname.startsWith("auto_backup_")) {
                backupCount++;
                if (oldestBackup == "" || fname < oldestBackup) {
                    oldestBackup = fname;
                }
            }
            file = root.openNextFile();
        }
        
        // 7'den fazla backup varsa en eskisini sil
        if (backupCount >= 7 && oldestBackup != "") {
            LittleFS.remove("/" + oldestBackup);
            addLog("🗑️ Eski backup silindi: " + oldestBackup, INFO, "BACKUP");
        }
        
        // Yeni backup oluştur
        if (saveBackupToFile(filename)) {
            lastBackup = millis();
            addLog("💾 Otomatik backup oluşturuldu", SUCCESS, "BACKUP");
        }
    }
}