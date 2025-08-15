// settings.cpp - Basitleştirilmiş ve Optimize Edilmiş

#include "settings.h"
#include "log_system.h"
#include "crypto_utils.h"
#include <Preferences.h>

WebServer server(80);
Settings settings;

void loadSettings() {
    Preferences prefs;
    prefs.begin("app-settings", true);  // Read-only

    // Network ayarları - Sabit IP kullan
    settings.local_IP.fromString("192.168.1.160");
    settings.gateway.fromString("192.168.1.1");
    settings.subnet.fromString("255.255.255.0");
    settings.primaryDNS.fromString("8.8.8.8");

    // Cihaz bilgileri
    settings.deviceName = prefs.getString("dev_name", "TEİAŞ EKLİM");
    settings.transformerStation = prefs.getString("tm_name", "Ankara TM");
    settings.username = prefs.getString("username", "admin");
    
    // BaudRate - Varsayılan 115200
    settings.currentBaudRate = prefs.getLong("baudrate", 115200);

    // Güvenlik - Basit şifre sistemi
    settings.passwordSalt = prefs.getString("p_salt", "");
    settings.passwordHash = prefs.getString("p_hash", "");

    // İlk kurulum - varsayılan şifre: 1234
    if (settings.passwordSalt.length() == 0) {
        settings.passwordSalt = "default_salt_12345";  // Sabit salt
        settings.passwordHash = sha256("1234", settings.passwordSalt);
    }

    prefs.end();

    // Session ayarları
    settings.isLoggedIn = false;
    settings.sessionStartTime = 0;
    settings.SESSION_TIMEOUT = 3600000; // 60 dakika (30 yerine)

    addLog("Ayarlar yüklendi", INFO, "SETTINGS");
}

bool saveSettings(const String& newDevName, const String& newTmName, 
                  const String& newUsername, const String& newPassword) {
    
    // Basit validasyon
    if (newDevName.length() < 3 || newDevName.length() > 50) return false;
    if (newUsername.length() < 3 || newUsername.length() > 30) return false;

    Preferences prefs;
    prefs.begin("app-settings", false);

    // Sadece değişenleri kaydet
    if (newDevName != settings.deviceName) {
        settings.deviceName = newDevName;
        prefs.putString("dev_name", newDevName);
    }

    if (newTmName != settings.transformerStation) {
        settings.transformerStation = newTmName;
        prefs.putString("tm_name", newTmName);
    }

    if (newUsername != settings.username) {
        settings.username = newUsername;
        prefs.putString("username", newUsername);
    }

    // Şifre değişikliği
    if (newPassword.length() >= 4) {
        settings.passwordSalt = "salt_" + String(millis());  // Basit salt
        settings.passwordHash = sha256(newPassword, settings.passwordSalt);
        prefs.putString("p_salt", settings.passwordSalt);
        prefs.putString("p_hash", settings.passwordHash);
        
        // Oturumu kapat
        settings.isLoggedIn = false;
        
        addLog("Şifre değiştirildi", INFO, "SETTINGS");
    }

    prefs.end();
    addLog("Ayarlar kaydedildi", SUCCESS, "SETTINGS");
    return true;
}

void initEthernet() {
    // WT32-ETH01 için sabit pinler
    ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
    
    // Statik IP kullan
    ETH.config(settings.local_IP, settings.gateway, settings.subnet, settings.primaryDNS);

    // Bağlantı kontrolü
    unsigned long start = millis();
    while (!ETH.linkUp() && millis() - start < 3000) {  // 3 saniye bekle
        delay(100);
    }
    
    if (ETH.linkUp()) {
        addLog("Ethernet OK: " + ETH.localIP().toString(), SUCCESS, "ETH");
    } else {
        addLog("Ethernet kablosu takılı değil", WARN, "ETH");
    }
}