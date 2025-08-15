// network_config.cpp
#include <ETH.h>
#include <Preferences.h>
#include "log_system.h"
#include "settings.h"

// Global settings değişkenini kullan
extern Settings settings;

// Network ayarlarını yükle
void loadNetworkConfig() {
    // Settings zaten loadSettings() içinde yükleniyor
    // Bu fonksiyon sadece uyumluluk için
    addLog("Network yapılandırması hazır", INFO, "NET");
}

// Ethernet başlatma - Gelişmiş versiyon
void initEthernetAdvanced() {
    addLog("Ethernet başlatılıyor...", INFO, "ETH");
    
    // WT32-ETH01 için doğru pinler
    ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
    
    // DHCP kullan (varsayılan)
    // Statik IP istenirse settings'den alınacak
    if (settings.local_IP != IPAddress(0,0,0,0)) {
        // Statik IP ayarla
        if (!ETH.config(settings.local_IP, settings.gateway, settings.subnet, settings.primaryDNS)) {
            addLog("❌ Statik IP atanamadı!", ERROR, "ETH");
        } else {
            addLog("✅ Statik IP: " + settings.local_IP.toString(), SUCCESS, "ETH");
        }
    } else {
        addLog("DHCP ile IP alınıyor...", INFO, "ETH");
    }
    
    // Bağlantı bekleme
    unsigned long startTime = millis();
    while (!ETH.linkUp() && millis() - startTime < 5000) {
        delay(100);
    }
    
    if (ETH.linkUp()) {
        // IP bilgilerini göster
        addLog("✅ Ethernet aktif", SUCCESS, "ETH");
        addLog("IP Adresi: " + ETH.localIP().toString(), INFO, "ETH");
        addLog("Gateway: " + ETH.gatewayIP().toString(), INFO, "ETH");
        addLog("Subnet: " + ETH.subnetMask().toString(), INFO, "ETH");
        addLog("DNS: " + ETH.dnsIP().toString(), INFO, "ETH");
        addLog("MAC: " + ETH.macAddress(), INFO, "ETH");
        
        // Hız ve duplex bilgisi
        addLog("Link Hızı: " + String(ETH.linkSpeed()) + " Mbps", INFO, "ETH");
        addLog("Duplex: " + String(ETH.fullDuplex() ? "Full" : "Half"), INFO, "ETH");
        
        // IP'yi settings'e kaydet (DHCP'den alındıysa)
        if (settings.local_IP == IPAddress(0,0,0,0)) {
            settings.local_IP = ETH.localIP();
            settings.gateway = ETH.gatewayIP();
            settings.subnet = ETH.subnetMask();
            settings.primaryDNS = ETH.dnsIP();
        }
    } else {
        addLog("⚠️ Ethernet kablosu bağlı değil", WARN, "ETH");
    }
}