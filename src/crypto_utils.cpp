#include "crypto_utils.h"
#include "mbedtls/sha256.h"
#include <Arduino.h>

String sha256(const String& data, const String& salt) {
    // Input validation
    if (data.length() == 0 || salt.length() == 0) {
        return "";
    }
    
    String toHash = salt + data; // Güvenlik için önce tuz, sonra parola
    byte hashResult[32];
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    
    // mbedtls_sha256_starts fonksiyonu void döndürür, int değil
    mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256
    
    // mbedtls_sha256_update fonksiyonu da void döndürür
    mbedtls_sha256_update(&ctx, (const unsigned char*) toHash.c_str(), toHash.length());
    
    // mbedtls_sha256_finish fonksiyonu da void döndürür
    mbedtls_sha256_finish(&ctx, hashResult);
    
    mbedtls_sha256_free(&ctx);

    // Hash'i onaltılık (hexadecimal) string'e çevir
    char hexString[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hexString + i * 2, "%02x", hashResult[i]);
    }
    hexString[64] = '\0';
    
    return String(hexString);
}

// Güvenli rastgele tuz üretici
String generateSalt(int length) {
    if (length <= 0 || length > 32) {
        length = 16; // Varsayılan uzunluk
    }
    
    String salt = "";
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    for (int i = 0; i < length; i++) {
        // ESP32'nin donanım rastgele sayı üreticisini kullan
        uint32_t randomNum = esp_random();
        salt += charset[randomNum % (sizeof(charset) - 1)];
    }
    
    return salt;
}

// Parola güçlülük kontrolü
bool isPasswordStrong(const String& password) {
    if (password.length() < 4) return false;
    
    bool hasUpper = false, hasLower = false, hasDigit = false;
    
    for (char c : password) {
        if (c >= 'A' && c <= 'Z') hasUpper = true;
        else if (c >= 'a' && c <= 'z') hasLower = true;
        else if (c >= '0' && c <= '9') hasDigit = true;
    }
    
    // En az 2 farklı karakter türü olsun
    int score = hasUpper + hasLower + hasDigit;
    return score >= 2;
}