#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <Arduino.h>

String sha256(const String& data, const String& salt);
String generateSalt(int length = 16);
bool isPasswordStrong(const String& password);

#endif