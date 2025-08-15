#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <Arduino.h>

// UART Protocol definitions
#define FRAME_START_CHAR    0x02  // STX
#define FRAME_END_CHAR      0x03  // ETX
#define FRAME_ESCAPE_CHAR   0x1B  // ESC
#define MAX_FRAME_SIZE      512
#define FRAME_TIMEOUT       2000

// Frame structure
struct UARTFrame {
    uint8_t command;
    uint16_t dataLength;
    uint8_t data[MAX_FRAME_SIZE];
    uint8_t checksum;
};

// Command codes
enum UARTCommands {
    CMD_GET_TIME = 0x10,
    CMD_SET_NTP = 0x11,
    CMD_GET_NTP = 0x12,
    CMD_GET_FIRST_FAULT = 0x20,
    CMD_GET_NEXT_FAULT = 0x21,
    CMD_CLEAR_FAULTS = 0x22,
    CMD_SET_BAUDRATE = 0x30,
    CMD_PING = 0x40,
    CMD_RESET = 0x50,
    CMD_GET_STATUS = 0x60,
    CMD_ACK = 0xA0,
    CMD_NACK = 0xA1
};

// Statistics structure
struct UARTStatistics {
    unsigned long totalFramesSent;
    unsigned long totalFramesReceived;
    unsigned long checksumErrors;
    unsigned long timeoutErrors;
    unsigned long frameErrors;
    float successRate;
};

// Global variables
extern UARTStatistics uartStats;
extern String lastResponse;
extern bool uartHealthy;

// Function declarations
uint8_t calculateCRC8(const uint8_t* data, size_t length);
uint8_t calculateXORChecksum(const uint8_t* data, size_t length);
bool createFrame(UARTFrame& frame, uint8_t command, const uint8_t* data, uint16_t dataLength);
bool sendFrame(const UARTFrame& frame);
bool receiveFrame(UARTFrame& frame, unsigned long timeout);
bool sendCommandWithProtocol(uint8_t command, const String& data, String& response, unsigned long timeout);
bool requestTimeWithProtocol();
bool sendNTPConfigWithProtocol(const String& server1, const String& server2);
bool requestFirstFaultWithProtocol();
bool requestNextFaultWithProtocol();
bool pingBackend();
void checkUARTHealthWithProtocol();
void updateUARTStatistics(bool success, bool checksumError = false, bool timeoutError = false);
String getUARTStatisticsJSON();

#endif // UART_PROTOCOL_H