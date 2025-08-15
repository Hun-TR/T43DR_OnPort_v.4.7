// ntp_handler.cpp
#include "ntp_handler.h"
#include "log_system.h"
#include "uart_handler.h"
#include <Preferences.h>

// Global değişkenler
NTPConfig ntpConfig;
bool ntpConfigured = false;

// NTP sunucu bilgilerini dsPIC33EP'den al
bool requestNTPFromBackend() {
    String response;
    
    // getNTP komutu gönder
    if (!sendCustomCommand("getNTP", response, 3000)) {
        addLog("❌ dsPIC33EP'den NTP bilgisi alınamadı", ERROR, "NTP");
        return false;
    }
    
    // Yanıt formatı: "NTP:server1,server2" veya sadece "server1,server2"
    if (response.startsWith("NTP:")) {
        response = response.substring(4);
    }
    
    int commaIndex = response.indexOf(',');
    if (commaIndex > 0) {
        String server1 = response.substring(0, commaIndex);
        String server2 = response.substring(commaIndex + 1);
        
        server1.trim();
        server2.trim();
        
        if (isValidIPOrDomain(server1) && isValidIPOrDomain(server2)) {
            server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
            server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
            
            addLog("✅ NTP sunucuları dsPIC33EP'den alındı: " + server1 + ", " + server2, SUCCESS, "NTP");
            return true;
        }
    }
    
    addLog("❌ Geçersiz NTP yanıt formatı: " + response, ERROR, "NTP");
    return false;
}

// NTP ayarlarını dsPIC33EP'ye gönder
void sendNTPConfigToBackend() {
    if (strlen(ntpConfig.ntpServer1) == 0) {
        addLog("NTP sunucu adresi boş", WARN, "NTP");
        return;
    }
    
    // Yeni format: "setNTP:server1,server2"
    String command = "setNTP:" + String(ntpConfig.ntpServer1) + "," + String(ntpConfig.ntpServer2);
    String response;
    
    if (sendCustomCommand(command, response, 2000)) {
        if (response == "ACK" || response.indexOf("OK") >= 0) {
            addLog("✅ NTP ayarları dsPIC33EP tarafından onaylandı", SUCCESS, "NTP");
        } else {
            addLog("dsPIC33EP yanıtı: " + response, WARN, "NTP");
        }
    } else {
        addLog("⚠️ NTP ayarları için yanıt alınamadı", WARN, "NTP");
    }
}

bool loadNTPSettings() {
    Preferences preferences;
    preferences.begin("ntp-config", true);
    
    String server1 = preferences.getString("ntp_server1", "");
    if (server1.length() == 0) {
        preferences.end();
        
        // Kayıtlı ayar yoksa dsPIC33EP'den iste
        if (requestNTPFromBackend()) {
            ntpConfigured = true;
            return true;
        }
        return false;
    }
    
    String server2 = preferences.getString("ntp_server2", "");
    
    if (!isValidIPOrDomain(server1) || (server2.length() > 0 && !isValidIPOrDomain(server2))) {
        preferences.end();
        return false;
    }
    
    server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
    server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
    
    ntpConfig.timezone = preferences.getInt("timezone", 3);
    ntpConfig.enabled = preferences.getBool("enabled", true);
    
    preferences.end();
    
    ntpConfigured = true;
    addLog("✅ NTP ayarları yüklendi", SUCCESS, "NTP");
    return true;
}

bool isValidIPOrDomain(const String& address) {
    if (address.length() < 7 || address.length() > 253) return false;
    
    // IP adresi kontrolü
    IPAddress testIP;
    if (testIP.fromString(address)) {
        return true;
    }
    
    // Domain adı kontrolü (basit)
    if (address.indexOf('.') > 0 && address.indexOf(' ') == -1) {
        return true;
    }
    
    return false;
}

bool saveNTPSettings(const String& server1, const String& server2, int timezone) {
    if (!isValidIPOrDomain(server1)) {
        addLog("Geçersiz birincil NTP sunucu", ERROR, "NTP");
        return false;
    }
    
    if (server2.length() > 0 && !isValidIPOrDomain(server2)) {
        addLog("Geçersiz ikincil NTP sunucu", ERROR, "NTP");
        return false;
    }
    
    Preferences preferences;
    preferences.begin("ntp-config", false);
    
    preferences.putString("ntp_server1", server1);
    preferences.putString("ntp_server2", server2);
    preferences.putInt("timezone", timezone);
    preferences.putBool("enabled", true);
    
    preferences.end();
    
    // Global config güncelle
    server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
    server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
    ntpConfig.timezone = timezone;
    ntpConfig.enabled = true;
    ntpConfigured = true;
    
    addLog("✅ NTP ayarları kaydedildi", SUCCESS, "NTP");
    
    // dsPIC33EP'ye gönder
    sendNTPConfigToBackend();
    return true;
}

// NTP Handler başlatma
void initNTPHandler() {
    // NTP ayarları yükleme
    if (!loadNTPSettings()) {
        addLog("⚠️ Kayıtlı NTP ayarı bulunamadı, varsayılanlar kullanılıyor", WARN, "NTP");
        // Varsayılan ayarları yükle
        strcpy(ntpConfig.ntpServer1, "pool.ntp.org");
        strcpy(ntpConfig.ntpServer2, "time.google.com");
        ntpConfig.timezone = 3;
        ntpConfig.enabled = true;
        ntpConfigured = false;
    }
    
    // İlk konfigürasyonu gönder
    delay(1000); // Backend'in hazır olmasını bekle
    sendNTPConfigToBackend();
    
    addLog("✅ NTP Handler başlatıldı", SUCCESS, "NTP");
}

// Eski fonksiyonları inline yap (çoklu tanımlama hatası için)
ReceivedTimeData receivedTime = {.date = "", .time = "", .isValid = false, .lastUpdate = 0};

void processReceivedData() {
    // Bu fonksiyon artık time_sync.cpp tarafından yönetiliyor
}

void readBackendData() {
    // Bu fonksiyon artık time_sync.cpp tarafından yönetiliyor  
}

void parseTimeData(const String& data) {
    // Bu fonksiyon artık time_sync.cpp tarafından yönetiliyor
}

// formatDate ve formatTime fonksiyonlarını inline yap veya kaldır
// time_sync.cpp'de zaten tanımlı olduğu için burada tanımlama

bool isTimeDataValid() {
    return false; // time_sync.cpp'deki isTimeSynced() kullanılacak
}

bool isNTPSynced() {
    return ntpConfigured;
}

void resetNTPSettings() {
    Preferences preferences;
    preferences.begin("ntp-config", false);
    preferences.clear();
    preferences.end();
    
    ntpConfigured = false;
    
    addLog("NTP ayarları sıfırlandı", INFO, "NTP");
}