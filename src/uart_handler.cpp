#include "uart_handler.h"
#include "uart_protocol.h"
#include "log_system.h"
#include "settings.h"
#include <Preferences.h>

// UART Pin tanımlamaları - DÜZELTME
#define UART_RX_PIN 5   // IO5 - RX2 (önceki: 4)
#define UART_TX_PIN 17  // IO17 - TX2 (önceki: 2)
#define UART_PORT   Serial2
#define UART_TIMEOUT 1000
#define MAX_RESPONSE_LENGTH 256

static unsigned long lastUARTActivity = 0;
static int uartErrorCount = 0;

void initUART() {
    // UART pinlerini başlat
    pinMode(UART_RX_PIN, INPUT);
    pinMode(UART_TX_PIN, OUTPUT);
    
    // Serial2'yi belirtilen pinlerle başlat
    UART_PORT.begin(settings.currentBaudRate, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    
    // Buffer'ı temizle
    while (UART_PORT.available()) {
        UART_PORT.read();
    }
    
    lastUARTActivity = millis();
    uartErrorCount = 0;
    uartHealthy = true;
    
    addLog("✅ UART başlatıldı - TX2: IO" + String(UART_TX_PIN) + 
           ", RX2: IO" + String(UART_RX_PIN) + 
           ", Baud: " + String(settings.currentBaudRate), SUCCESS, "UART");
}

// dsPIC33EP'ye sadece baudrate KODU gönder (cihazın kendi baudrate'i değişmeyecek)
bool sendBaudRateCommand(long baudRate) {
    String command = "";
    
    // dsPIC'e gönderilecek baudrate kodları
    switch(baudRate) {
        case 9600:   command = "br9600";   break;
        case 19200:  command = "br19200";  break;
        case 38400:  command = "br38400";  break;
        case 57600:  command = "br57600";  break;
        case 115200: command = "br115200"; break;
        default:
            addLog("Geçersiz baudrate kodu: " + String(baudRate), ERROR, "UART");
            return false;
    }
    
    // Buffer'ı temizle
    while (UART_PORT.available()) {
        UART_PORT.read();
    }
    
    // Komutu gönder
    UART_PORT.println(command);
    UART_PORT.flush();
    
    addLog("dsPIC33EP'ye baudrate kodu gönderildi: " + command, INFO, "UART");
    
    // ACK bekle
    String response = safeReadUARTResponse(2000);
    
    if (response == "ACK" || response.indexOf("OK") >= 0) {
        addLog("✅ Baudrate kodu dsPIC33EP tarafından alındı", SUCCESS, "UART");
        return true;
    } else if (response.length() > 0) {
        addLog("dsPIC33EP yanıtı: " + response, WARN, "UART");
        return true; // Yanıt varsa başarılı say
    } else {
        addLog("❌ dsPIC33EP'den yanıt alınamadı", ERROR, "UART");
        return false;
    }
}

// changeBaudRate fonksiyonu - sadece kod gönderir, ESP32 baudrate'i değişmez
bool changeBaudRate(long baudRate) {
    // Bu fonksiyon artık sadece sendBaudRateCommand'ı çağırıyor
    // ESP32'nin kendi baudrate'i değişmeyecek
    return sendBaudRateCommand(baudRate);
}

// Güvenli UART okuma
String safeReadUARTResponse(unsigned long timeout) {
    String response = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < timeout) {
        if (UART_PORT.available()) {
            char c = UART_PORT.read();
            lastUARTActivity = millis();
            uartHealthy = true;
            
            if (c == '\n' || c == '\r') {
                if (response.length() > 0) {
                    return response;
                }
            } else if (c >= 32 && c <= 126) { // Yazdırılabilir karakterler
                response += c;
                if (response.length() >= MAX_RESPONSE_LENGTH - 1) {
                    return response;
                }
            }
        }
        delay(1);
    }
    
    return response;
}

// Arıza kayıtları için komutlar
bool requestFirstFault() {
    while (UART_PORT.available()) {
        UART_PORT.read();
    }
    
    String command = "12345v"; // İlk arıza komutu
    UART_PORT.println(command);
    UART_PORT.flush();
    
    addLog("Arıza sorgu komutu: " + command, DEBUG, "UART");
    
    lastResponse = safeReadUARTResponse(UART_TIMEOUT);
    
    if (lastResponse.length() > 0) {
        addLog("Arıza kaydı alındı: " + lastResponse.substring(0, 20) + "...", DEBUG, "UART");
        return true;
    }
    
    uartErrorCount++;
    return false;
}

bool requestNextFault() {
    while (UART_PORT.available()) {
        UART_PORT.read();
    }
    
    String command = "n"; // Sonraki arıza komutu
    UART_PORT.println(command);
    UART_PORT.flush();
    
    lastResponse = safeReadUARTResponse(UART_TIMEOUT);
    
    if (lastResponse.length() > 0) {
        return true;
    }
    
    uartErrorCount++;
    return false;
}

String getLastFaultResponse() {
    return lastResponse;
}

// UART sağlık kontrolü
void checkUARTHealth() {
    if (millis() - lastUARTActivity > 300000 && uartHealthy) { // 5 dakika
        addLog("⚠️ UART 5 dakikadır sessiz", WARN, "UART");
        uartHealthy = false;
    }
    
    if (uartErrorCount > 10) {
        addLog("🔄 UART yeniden başlatılıyor...", WARN, "UART");
        initUART();
        uartErrorCount = 0;
    }
}

// UART durumu
String getUARTStatus() {
    return uartHealthy ? "Aktif" : "Pasif";
}

// Özel komut gönderme (NTP ve diğer komutlar için)
bool sendCustomCommand(const String& command, String& response, unsigned long timeout) {
    if (command.length() == 0 || command.length() > 100) {
        return false;
    }
    
    while (UART_PORT.available()) {
        UART_PORT.read();
    }
    
    UART_PORT.println(command);
    UART_PORT.flush();
    
    response = safeReadUARTResponse(timeout == 0 ? UART_TIMEOUT : timeout);
    
    return response.length() > 0;
}

// Test fonksiyonu
bool testUARTConnection() {
    addLog("UART bağlantı testi...", INFO, "UART");
    
    String response;
    bool result = sendCustomCommand("TEST", response, 1000);
    
    if (result) {
        addLog("✅ UART testi başarılı: " + response, SUCCESS, "UART");
    } else {
        addLog("❌ UART testi başarısız", ERROR, "UART");
    }
    
    return result;
}