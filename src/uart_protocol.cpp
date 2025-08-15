#include "uart_protocol.h"
#include "uart_handler.h"
#include "log_system.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// Global değişkenler (header'da extern olarak tanımlı)
String lastResponse = "";
bool uartHealthy = true;
UARTStatistics uartStats = {0, 0, 0, 0, 0, 100.0};

// Frame durumları (local enum)
enum FrameState {
    WAIT_START,
    READ_COMMAND,
    READ_LENGTH_HIGH,
    READ_LENGTH_LOW,
    READ_DATA,
    READ_CHECKSUM,
    WAIT_END
};

// CRC8 checksum hesaplama
uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07; // CRC-8 polynomial
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

// XOR checksum hesaplama (basit ve hızlı)
uint8_t calculateXORChecksum(const uint8_t* data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Frame oluşturma
bool createFrame(UARTFrame& frame, uint8_t command, const uint8_t* data, uint16_t dataLength) {
    if (dataLength > MAX_FRAME_SIZE) {
        addLog("❌ Frame verisi çok büyük: " + String(dataLength), ERROR, "UART");
        return false;
    }
    
    frame.command = command;
    frame.dataLength = dataLength;
    
    if (data != nullptr && dataLength > 0) {
        memcpy(frame.data, data, dataLength);
    }
    
    // Checksum hesapla (command + length + data)
    uint8_t checksumData[MAX_FRAME_SIZE + 3];
    checksumData[0] = command;
    checksumData[1] = (dataLength >> 8) & 0xFF;
    checksumData[2] = dataLength & 0xFF;
    
    if (dataLength > 0) {
        memcpy(&checksumData[3], data, dataLength);
    }
    
    frame.checksum = calculateXORChecksum(checksumData, dataLength + 3);
    
    return true;
}

// Frame gönderme (escape karakterleri ile)
bool sendFrame(const UARTFrame& frame) {
    // Frame başlangıcı
    Serial2.write(FRAME_START_CHAR);
    
    // Command gönder
    if (frame.command == FRAME_START_CHAR || frame.command == FRAME_END_CHAR || frame.command == FRAME_ESCAPE_CHAR) {
        Serial2.write(FRAME_ESCAPE_CHAR);
    }
    Serial2.write(frame.command);
    
    // Length gönder (2 byte, big-endian)
    uint8_t lengthHigh = (frame.dataLength >> 8) & 0xFF;
    uint8_t lengthLow = frame.dataLength & 0xFF;
    
    if (lengthHigh == FRAME_START_CHAR || lengthHigh == FRAME_END_CHAR || lengthHigh == FRAME_ESCAPE_CHAR) {
        Serial2.write(FRAME_ESCAPE_CHAR);
    }
    Serial2.write(lengthHigh);
    
    if (lengthLow == FRAME_START_CHAR || lengthLow == FRAME_END_CHAR || lengthLow == FRAME_ESCAPE_CHAR) {
        Serial2.write(FRAME_ESCAPE_CHAR);
    }
    Serial2.write(lengthLow);
    
    // Data gönder
    for (uint16_t i = 0; i < frame.dataLength; i++) {
        if (frame.data[i] == FRAME_START_CHAR || frame.data[i] == FRAME_END_CHAR || frame.data[i] == FRAME_ESCAPE_CHAR) {
            Serial2.write(FRAME_ESCAPE_CHAR);
        }
        Serial2.write(frame.data[i]);
    }
    
    // Checksum gönder
    if (frame.checksum == FRAME_START_CHAR || frame.checksum == FRAME_END_CHAR || frame.checksum == FRAME_ESCAPE_CHAR) {
        Serial2.write(FRAME_ESCAPE_CHAR);
    }
    Serial2.write(frame.checksum);
    
    // Frame sonu
    Serial2.write(FRAME_END_CHAR);
    
    Serial2.flush();
    
    addLog("📤 Frame gönderildi - Cmd: 0x" + String(frame.command, HEX) + ", Len: " + String(frame.dataLength), DEBUG, "UART");
    
    return true;
}

// Frame okuma (state machine ile)
bool receiveFrame(UARTFrame& frame, unsigned long timeout) {
    FrameState state = WAIT_START;
    unsigned long startTime = millis();
    uint16_t dataIndex = 0;
    bool escapeNext = false;
    uint8_t checksumData[MAX_FRAME_SIZE + 3];
    uint16_t checksumIndex = 0;
    
    while (millis() - startTime < timeout) {
        if (Serial2.available()) {
            uint8_t byte = Serial2.read();
            
            // Escape karakteri kontrolü
            if (byte == FRAME_ESCAPE_CHAR && !escapeNext) {
                escapeNext = true;
                continue;
            }
            
            // Escape sonrası karakter
            if (escapeNext) {
                escapeNext = false;
                // Byte'ı normal olarak işle
            } else {
                // Start/End karakterlerini kontrol et
                if (byte == FRAME_START_CHAR) {
                    state = READ_COMMAND;
                    dataIndex = 0;
                    checksumIndex = 0;
                    continue;
                } else if (byte == FRAME_END_CHAR && state == READ_CHECKSUM) {
                    // Frame tamamlandı, checksum kontrolü yap
                    uint8_t calculatedChecksum = calculateXORChecksum(checksumData, checksumIndex);
                    
                    if (calculatedChecksum == frame.checksum) {
                        addLog("✅ Frame alındı - Cmd: 0x" + String(frame.command, HEX) + ", Len: " + String(frame.dataLength), DEBUG, "UART");
                        return true;
                    } else {
                        addLog("❌ Checksum hatası! Beklenen: 0x" + String(calculatedChecksum, HEX) + ", Alınan: 0x" + String(frame.checksum, HEX), ERROR, "UART");
                        return false;
                    }
                }
            }
            
            // State machine
            switch (state) {
                case WAIT_START:
                    // Start karakteri bekleniyor
                    break;
                    
                case READ_COMMAND:
                    frame.command = byte;
                    checksumData[checksumIndex++] = byte;
                    state = READ_LENGTH_HIGH;
                    break;
                    
                case READ_LENGTH_HIGH:
                    frame.dataLength = (byte << 8);
                    checksumData[checksumIndex++] = byte;
                    state = READ_LENGTH_LOW;
                    break;
                    
                case READ_LENGTH_LOW:
                    frame.dataLength |= byte;
                    checksumData[checksumIndex++] = byte;
                    
                    if (frame.dataLength > MAX_FRAME_SIZE) {
                        addLog("❌ Frame verisi çok büyük: " + String(frame.dataLength), ERROR, "UART");
                        return false;
                    }
                    
                    if (frame.dataLength > 0) {
                        state = READ_DATA;
                        dataIndex = 0;
                    } else {
                        state = READ_CHECKSUM;
                    }
                    break;
                    
                case READ_DATA:
                    frame.data[dataIndex] = byte;
                    checksumData[checksumIndex++] = byte;
                    dataIndex++;
                    
                    if (dataIndex >= frame.dataLength) {
                        state = READ_CHECKSUM;
                    }
                    break;
                    
                case READ_CHECKSUM:
                    frame.checksum = byte;
                    state = WAIT_END;
                    break;
                    
                case WAIT_END:
                    // End karakteri bekleniyor (yukarıda kontrol ediliyor)
                    break;
            }
        }
        
        delay(1);
    }
    
    addLog("⏱️ Frame okuma timeout", WARN, "UART");
    return false;
}

// Komut gönder ve yanıt al (yeni protokol ile)
bool sendCommandWithProtocol(uint8_t command, const String& data, String& response, unsigned long timeout) {
    UARTFrame txFrame, rxFrame;
    
    // TX frame oluştur
    if (!createFrame(txFrame, command, (uint8_t*)data.c_str(), data.length())) {
        return false;
    }
    
    // Frame gönder
    if (!sendFrame(txFrame)) {
        return false;
    }
    
    // Yanıt bekle
    if (!receiveFrame(rxFrame, timeout)) {
        return false;
    }
    
    // Yanıtı string'e çevir
    response = "";
    for (uint16_t i = 0; i < rxFrame.dataLength; i++) {
        response += (char)rxFrame.data[i];
    }
    
    return true;
}

// Gelişmiş komut gönderme fonksiyonları
bool requestTimeWithProtocol() {
    String response;
    if (sendCommandWithProtocol(CMD_GET_TIME, "", response, 2000)) {
        // Response formatı: "DDMMYYHHMMSS"
        if (response.length() == 12) {
            addLog("✅ Zaman bilgisi alındı: " + response, SUCCESS, "UART");
            return true;
        }
    }
    return false;
}

bool sendNTPConfigWithProtocol(const String& server1, const String& server2) {
    String data = server1 + "," + server2;
    String response;
    
    if (sendCommandWithProtocol(CMD_SET_NTP, data, response, 2000)) {
        if (response == "ACK") {
            addLog("✅ NTP config gönderildi", SUCCESS, "UART");
            return true;
        }
    }
    return false;
}

bool requestFirstFaultWithProtocol() {
    String response;
    if (sendCommandWithProtocol(CMD_GET_FIRST_FAULT, "", response, 3000)) {
        if (response.length() > 0) {
            addLog("✅ İlk arıza kaydı alındı", SUCCESS, "UART");
            // Response'u global değişkene kaydet
            lastResponse = response;
            return true;
        }
    }
    return false;
}

bool requestNextFaultWithProtocol() {
    String response;
    if (sendCommandWithProtocol(CMD_GET_NEXT_FAULT, "", response, 3000)) {
        if (response.length() > 0) {
            addLog("✅ Sonraki arıza kaydı alındı", SUCCESS, "UART");
            lastResponse = response;
            return true;
        }
    }
    return false;
}

// Ping komutu - bağlantı testi
bool pingBackend() {
    String response;
    if (sendCommandWithProtocol(CMD_PING, "PING", response, 1000)) {
        if (response == "PONG") {
            return true;
        }
    }
    return false;
}

// UART sağlık kontrolü - geliştirilmiş
void checkUARTHealthWithProtocol() {
    static unsigned long lastPing = 0;
    static int consecutiveFailures = 0;
    const unsigned long PING_INTERVAL = 30000; // 30 saniye
    
    if (millis() - lastPing > PING_INTERVAL) {
        lastPing = millis();
        
        if (pingBackend()) {
            consecutiveFailures = 0;
            if (!uartHealthy) {
                uartHealthy = true;
                addLog("✅ UART bağlantısı düzeldi", SUCCESS, "UART");
            }
        } else {
            consecutiveFailures++;
            addLog("⚠️ UART ping başarısız (#" + String(consecutiveFailures) + ")", WARN, "UART");
            
            if (consecutiveFailures >= 3) {
                uartHealthy = false;
                addLog("❌ UART bağlantısı kayıp", ERROR, "UART");
                
                // UART'ı yeniden başlat
                if (consecutiveFailures >= 5) {
                    initUART();
                    consecutiveFailures = 0;
                }
            }
        }
    }
}

// İstatistikleri güncelle (default parametreler header'da tanımlı)
void updateUARTStatistics(bool success, bool checksumError, bool timeoutError) {
    if (success) {
        uartStats.totalFramesReceived++;
    } else {
        if (checksumError) uartStats.checksumErrors++;
        if (timeoutError) uartStats.timeoutErrors++;
        else uartStats.frameErrors++;
    }
    
    // Başarı oranını hesapla
    unsigned long total = uartStats.totalFramesSent + uartStats.totalFramesReceived;
    if (total > 0) {
        unsigned long successful = uartStats.totalFramesReceived;
        uartStats.successRate = (float)successful / (float)total * 100.0;
    }
}

// İstatistikleri JSON olarak döndür
String getUARTStatisticsJSON() {
    JsonDocument doc;  // Yeni ArduinoJson v7 syntax
    doc["totalSent"] = uartStats.totalFramesSent;
    doc["totalReceived"] = uartStats.totalFramesReceived;
    doc["checksumErrors"] = uartStats.checksumErrors;
    doc["timeoutErrors"] = uartStats.timeoutErrors;
    doc["frameErrors"] = uartStats.frameErrors;
    doc["successRate"] = uartStats.successRate;
    doc["healthy"] = uartHealthy;
    
    String output;
    serializeJson(doc, output);
    return output;
}