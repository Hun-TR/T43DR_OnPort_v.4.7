#ifndef PASSWORD_POLICY_H
#define PASSWORD_POLICY_H

#include <Arduino.h>

// Password policy structure
struct PasswordPolicy {
    bool firstLoginPasswordChange;
    bool passwordExpiry;
    int passwordExpiryDays;
    int minPasswordLength;
    bool requireComplexPassword;
    unsigned long lastPasswordChange;
    bool isDefaultPassword;
    int passwordHistory;
};

// Global password policy
extern PasswordPolicy passwordPolicy;

// Function declarations
void loadPasswordPolicy();
void savePasswordPolicy();
bool isPasswordComplex(const String& password);
bool isPasswordInHistory(const String& password);
void addPasswordToHistory(const String& passwordHash, const String& salt);
bool isPasswordExpired();
bool mustChangePassword();
void handlePasswordChangePage();
void handlePasswordChangeAPI();

#endif // PASSWORD_POLICY_H