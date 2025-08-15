// time_sync.cpp
#include "uart_handler.h"
#include "log_system.h"
#include <time.h>

// Global zaman değişkenleri
struct TimeData {
    bool isValid;
    String lastDate;
    String lastTime;
    unsigned long lastSync;
    int syncCount;
} timeData = {false, "", "", 0, 0};

// Forward declarations - Fonksiyon prototipleri
bool parseTimeResponse(const String& response);
String formatDate(const String& dateStr);
String formatTime(const String& timeStr);
void updateSystemTime();

// Tarih formatla: DDMMYY -> DD.MM.20YY
String formatDate(const String& dateStr) {
    if (dateStr.length() != 6) return "Geçersiz";
    
    int day = dateStr.substring(0, 2).toInt();
    int month = dateStr.substring(2, 4).toInt();
    int year = 2000 + dateStr.substring(4, 6).toInt();
    
    if (day < 1 || day > 31 || month < 1 || month > 12) {
        return "Geçersiz";
    }
    
    char buffer[12];
    sprintf(buffer, "%02d.%02d.%04d", day, month, year);
    return String(buffer);
}

// Saat formatla: HHMMSS -> HH:MM:SS
String formatTime(const String& timeStr) {
    if (timeStr.length() != 6) return "Geçersiz";
    
    int hour = timeStr.substring(0, 2).toInt();
    int minute = timeStr.substring(2, 4).toInt();
    int second = timeStr.substring(4, 6).toInt();
    
    if (hour > 23 || minute > 59 || second > 59) {
        return "Geçersiz";
    }
    
    char buffer[10];
    sprintf(buffer, "%02d:%02d:%02d", hour, minute, second);
    return String(buffer);
}

// ESP32 sistem saatini güncelle
void updateSystemTime() {
    if (!timeData.isValid) return;
    
    // Tarih ve saati parse et
    int day, month, year, hour, minute, second;
    sscanf(timeData.lastDate.c_str(), "%d.%d.%d", &day, &month, &year);
    sscanf(timeData.lastTime.c_str(), "%d:%d:%d", &hour, &minute, &second);
    
    // tm struct oluştur
    struct tm timeinfo;
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_isdst = 0;
    
    // Sistem saatini ayarla
    time_t t = mktime(&timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    
    addLog("Sistem saati güncellendi", INFO, "TIME");
}

// dsPIC'ten gelen zaman verisini parse et
bool parseTimeResponse(const String& response) {
    // Format 1: "DATE:DDMMYY,TIME:HHMMSS"
    if (response.indexOf("DATE:") >= 0 && response.indexOf("TIME:") >= 0) {
        int dateStart = response.indexOf("DATE:") + 5;
        int dateEnd = response.indexOf(",");
        int timeStart = response.indexOf("TIME:") + 5;
        
        String dateStr = response.substring(dateStart, dateEnd);
        String timeStr = response.substring(timeStart, timeStart + 6);
        
        if (dateStr.length() == 6 && timeStr.length() == 6) {
            timeData.lastDate = formatDate(dateStr);
            timeData.lastTime = formatTime(timeStr);
            return true;
        }
    }
    
    // Format 2: "DDMMYYHHMMSS" (12 karakter)
    if (response.length() == 12) {
        String dateStr = response.substring(0, 6);
        String timeStr = response.substring(6, 12);
        
        timeData.lastDate = formatDate(dateStr);
        timeData.lastTime = formatTime(timeStr);
        return true;
    }
    
    // Format 3: Checksum'lı veri "DDMMYYx" ve "HHMMSSy"
    if (response.length() == 7) {
        String dataOnly = response.substring(0, 6);
        char checksum = response.charAt(6);
        
        if (checksum >= 'A' && checksum <= 'Z') { // Tarih
            timeData.lastDate = formatDate(dataOnly);
            return true;
        } else if (checksum >= 'a' && checksum <= 'z') { // Saat
            timeData.lastTime = formatTime(dataOnly);
            timeData.isValid = true;
            return true;
        }
    }
    
    addLog("Geçersiz zaman formatı: " + response, WARN, "TIME");
    return false;
}

// dsPIC'ten zaman isteği gönder
bool requestTimeFromDsPIC() {
    String response;
    
    // Zaman isteği komutu gönder
    if (!sendCustomCommand("GETTIME", response, 2000)) {
        addLog("❌ dsPIC'ten zaman bilgisi alınamadı", ERROR, "TIME");
        return false;
    }
    
    // Yanıt formatı: "DATE:DDMMYY,TIME:HHMMSS" veya "DDMMYYHHMMSS"
    if (parseTimeResponse(response)) {
        timeData.lastSync = millis();
        timeData.syncCount++;
        timeData.isValid = true;
        
        addLog("✅ Zaman senkronize edildi: " + timeData.lastDate + " " + timeData.lastTime, SUCCESS, "TIME");
        
        // Sistem saatini güncelle
        updateSystemTime();
        return true;
    }
    
    return false;
}

// Periyodik senkronizasyon kontrolü (5 dakikada bir)
void checkTimeSync() {
    static unsigned long lastSyncRequest = 0;
    const unsigned long SYNC_INTERVAL = 300000; // 5 dakika
    
    unsigned long now = millis();
    
    // İlk senkronizasyon veya periyodik senkronizasyon
    if (timeData.syncCount == 0 || (now - lastSyncRequest > SYNC_INTERVAL)) {
        lastSyncRequest = now;
        requestTimeFromDsPIC();
    }
    
    // Zaman geçerliliğini kontrol et (10 dakika timeout)
    if (timeData.isValid && (now - timeData.lastSync > 600000)) {
        timeData.isValid = false;
        addLog("⚠️ Zaman senkronizasyonu kaybedildi", WARN, "TIME");
    }
}

// API için zaman bilgilerini döndür
String getCurrentDateTime() {
    if (!timeData.isValid) {
        return "Senkronizasyon bekleniyor...";
    }
    return timeData.lastDate + " " + timeData.lastTime;
}

String getCurrentDate() {
    return timeData.isValid ? timeData.lastDate : "---";
}

String getCurrentTime() {
    return timeData.isValid ? timeData.lastTime : "---";
}

bool isTimeSynced() {
    return timeData.isValid;
}

// Zaman senkronizasyon istatistikleri
String getTimeSyncStats() {
    String stats = "Senkronizasyon Durumu: ";
    stats += timeData.isValid ? "Aktif\n" : "Pasif\n";
    stats += "Toplam Senkronizasyon: " + String(timeData.syncCount) + "\n";
    
    if (timeData.lastSync > 0) {
        unsigned long elapsed = (millis() - timeData.lastSync) / 1000;
        stats += "Son Senkronizasyon: " + String(elapsed) + " saniye önce\n";
    }
    
    stats += "Son Tarih: " + timeData.lastDate + "\n";
    stats += "Son Saat: " + timeData.lastTime;
    
    return stats;
}