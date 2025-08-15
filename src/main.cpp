#include <Arduino.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <esp_log.h>
#include "settings.h"
#include "log_system.h"
#include "uart_handler.h"
#include "web_routes.h"
#include "websocket_handler.h"   // Yeni eklenen
#include "password_policy.h"     // Yeni eklenen
#include "backup_restore.h"      // Yeni eklenen
// HTTPS desteği şimdilik devre dışı (kütüphane uyumsuzluğu)

// External fonksiyonlar - time_sync.cpp
extern void checkTimeSync();
extern String getTimeSyncStats();

// External fonksiyonlar - network_config.cpp  
extern void loadNetworkConfig();
extern void initEthernetAdvanced();

// Task handle'ları
TaskHandle_t webTaskHandle = NULL;
TaskHandle_t uartTaskHandle = NULL;

// Sistem değişkenleri
unsigned long lastHeapCheck = 0;
size_t minFreeHeap = SIZE_MAX;

// Web server task - Core 0'da çalışacak
void webServerTask(void *parameter) {
    while(true) {
        server.handleClient();
        // mDNS.update() ESP32'de gerekli değil
        vTaskDelay(1);
    }
}

// UART ve zaman senkronizasyon task - Core 1'de
void uartTask(void *parameter) {
    while(true) {
        // Zaman senkronizasyonu kontrolü (5 dakikada bir)
        checkTimeSync();
        
        // UART sağlık kontrolü
        checkUARTHealth();
        
        vTaskDelay(1000); // 1 saniye
    }
}

// mDNS başlatma
void initMDNS() {
    // Cihaz adını oluştur (MAC adresinin son 2 baytı ile)
    uint8_t mac[6];
    ETH.macAddress(mac);
    char hostname[32];
    sprintf(hostname, "teias-%02x%02x", mac[4], mac[5]);
    
    if (MDNS.begin(hostname)) {
        addLog("✅ mDNS başlatıldı: " + String(hostname) + ".local", SUCCESS, "mDNS");
        
        // HTTP servisini duyur
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "device", "TEİAŞ EKLİM");
        MDNS.addServiceTxt("http", "tcp", "version", "3.0");
        
        Serial.println("\n╔════════════════════════════════════════╗");
        Serial.println("║         BAĞLANTI BİLGİLERİ             ║");
        Serial.println("╠════════════════════════════════════════╣");
        Serial.print("║ IP Adresi    : ");
        Serial.print(ETH.localIP().toString());
        for(int i = ETH.localIP().toString().length(); i < 24; i++) Serial.print(" ");
        Serial.println("║");
        Serial.print("║ mDNS Adresi  : http://");
        Serial.print(hostname);
        Serial.print(".local");
        for(int i = strlen(hostname) + 13; i < 24; i++) Serial.print(" ");
        Serial.println("║");
        Serial.print("║ MAC Adresi   : ");
        Serial.print(ETH.macAddress());
        for(int i = ETH.macAddress().length(); i < 24; i++) Serial.print(" ");
        Serial.println("║");
        Serial.println("╚════════════════════════════════════════╝\n");
        
    } else {
        addLog("❌ mDNS başlatılamadı", ERROR, "mDNS");
    }
}

void setup() {
    Serial.begin(115200);
    setCpuFrequencyMhz(240);
    
    // Debug loglarını kapat
    esp_log_level_set("*", ESP_LOG_NONE);
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║      TEİAŞ EKLİM SİSTEMİ v3.0          ║");
    Serial.println("║   Trafo Merkezi Arıza Kayıt Sistemi    ║");
    Serial.println("╚════════════════════════════════════════╝");
    
    Serial.print("\n► CPU Frekansı: ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" MHz");
    
    // LittleFS başlat
    Serial.print("► Dosya Sistemi (LittleFS)... ");
    if(!LittleFS.begin(true)){
        Serial.println("❌ HATA!");
        ESP.restart();
        return;
    }
    Serial.println("✅");
    
    // Modülleri başlat
    Serial.println("\n═══ MODÜLLER BAŞLATILIYOR ═══");
    
    Serial.print("► Log Sistemi... ");
    initLogSystem();
    Serial.println("✅");
    
    Serial.print("► Ayarlar... ");
    loadSettings();
    Serial.println("✅");
    
    Serial.print("► Network Yapılandırması... ");
    loadNetworkConfig();
    Serial.println("✅");
    
    Serial.print("► Ethernet... ");
    initEthernetAdvanced();
    Serial.println("✅");
    
    Serial.print("► UART (TX2:IO17, RX2:IO5)... ");
    initUART();
    Serial.println("✅");
    
    Serial.print("► Web Sunucu... ");
    setupWebRoutes();
    Serial.println("✅");
    
    Serial.print("► WebSocket Server... ");
    initWebSocket();
    Serial.println("✅");
    
    Serial.print("► Parola Politikası... ");
    loadPasswordPolicy();
    Serial.println("✅");
    
    Serial.print("► mDNS... ");
    initMDNS();
    
    // Multi-core task'ları başlat
    xTaskCreatePinnedToCore(
        webServerTask,
        "WebServer",
        8192,
        NULL,
        2,
        &webTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        uartTask,
        "UART",
        4096,
        NULL,
        1,
        &uartTaskHandle,
        1  // Core 1
    );
    
    minFreeHeap = ESP.getFreeHeap();
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║         SİSTEM HAZIR!                  ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ Kullanıcı: admin                       ║");
    Serial.println("║ Şifre    : 1234                        ║");
    Serial.print("║ Bellek   : ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes");
    for(int i = String(ESP.getFreeHeap()).length(); i < 24; i++) Serial.print(" ");
    Serial.println("║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    addLog("🚀 Sistem başlatıldı", SUCCESS, "SYSTEM");
    addLog("📍 Trafo Merkezi: " + settings.transformerStation, INFO, "SYSTEM");
}

void checkSystemHealth() {
    size_t currentHeap = ESP.getFreeHeap();
    
    if (currentHeap < minFreeHeap) {
        minFreeHeap = currentHeap;
    }
    
    if (currentHeap < 10000) {
        addLog("⚠️ Düşük bellek: " + String(currentHeap), WARN, "SYSTEM");
    }
    
    if (currentHeap < 5000) {
        ESP.restart();
    }
}

void loop() {
    unsigned long now = millis();
    
    // WebSocket handling
    handleWebSocket();
    
    // Automatic backup - her 24 saatte bir
    static unsigned long lastBackupCheck = 0;
    if (now - lastBackupCheck > 3600000) { // Her saat kontrol et
        createAutomaticBackup();
        lastBackupCheck = now;
    }
    
    // Sistem sağlık kontrolü - 10 saniyede bir
    if (now - lastHeapCheck > 10000) {
        checkSystemHealth();
        lastHeapCheck = now;
    }
    
    // Ethernet durumu kontrolü - 30 saniyede bir
    static unsigned long lastEthCheck = 0;
    static bool lastEthStatus = false;
    
    if (now - lastEthCheck > 30000) {
        bool currentEthStatus = ETH.linkUp();
        
        if (currentEthStatus != lastEthStatus) {
            if (currentEthStatus) {
                addLog("✅ Ethernet bağlandı", SUCCESS, "ETH");
                
                // Bağlantı bilgilerini logla
                addLog("IP: " + ETH.localIP().toString(), INFO, "ETH");
                addLog("Hız: " + String(ETH.linkSpeed()) + " Mbps", INFO, "ETH");
            } else {
                addLog("❌ Ethernet kesildi", ERROR, "ETH");
            }
            lastEthStatus = currentEthStatus;
        }
        lastEthCheck = now;
    }
    
    // Session timeout kontrolü
    if (settings.isLoggedIn) {
        if (millis() - settings.sessionStartTime > settings.SESSION_TIMEOUT) {
            settings.isLoggedIn = false;
            addLog("Oturum zaman aşımı", INFO, "AUTH");
        }
    }
    
    // Zaman senkronizasyon durumunu logla - 1 saatte bir
    static unsigned long lastTimeSyncLog = 0;
    if (now - lastTimeSyncLog > 3600000) { // 1 saat
        addLog(getTimeSyncStats(), INFO, "TIME");
        lastTimeSyncLog = now;
    }
    
    // İlk giriş sonrası parola değiştirme kontrolü
    static bool passwordChangeChecked = false;
    if (settings.isLoggedIn && !passwordChangeChecked) {
        if (mustChangePassword()) {
            // WebSocket üzerinden bildirim gönder
            broadcastLog("Parolanızı değiştirmeniz gerekmektedir", "WARNING", "AUTH");
        }
        passwordChangeChecked = true;
    }
    
    // WebSocket broadcast - her 5 saniyede bir durum güncellemesi
    static unsigned long lastBroadcast = 0;
    if (now - lastBroadcast > 5000) {
        if (isWebSocketConnected()) {
            broadcastStatus();
        }
        lastBroadcast = now;
    }
    
    vTaskDelay(100); // 100ms
}