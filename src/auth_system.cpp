#include "auth_system.h"
#include "settings.h"
#include "log_system.h"
#include "crypto_utils.h"
#include <WebServer.h>

extern Settings settings;
extern WebServer server;

// Giriş denemesi sayacı ve kilitlenme sistemi
static int loginAttempts = 0;
static unsigned long lockoutTime = 0;
const int MAX_LOGIN_ATTEMPTS = 5;
const unsigned long LOCKOUT_DURATION = 300000; // 5 dakika

bool checkSession() {
    if (!settings.isLoggedIn) return false;
    if (millis() - settings.sessionStartTime > settings.SESSION_TIMEOUT) {
        settings.isLoggedIn = false;
        addLog("Oturum zaman aşımı", INFO, "AUTH");
        return false;
    }
    return true;
}

void handleUserLogin() {
    // Rate limiting kontrolü
    if (lockoutTime > 0 && millis() < lockoutTime) {
        unsigned long remainingTime = (lockoutTime - millis()) / 1000;
        addLog("Çok fazla başarısız giriş denemesi. Kalan süre: " + String(remainingTime) + "s", WARN, "AUTH");
        server.send(429, "application/json", 
            "{\"error\":\"Çok fazla başarısız deneme. " + String(remainingTime) + " saniye sonra tekrar deneyin.\"}");
        return;
    }

    String u = server.arg("username");
    String p = server.arg("password");

    // Input validation
    if (u.length() == 0 || p.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"Kullanıcı adı ve şifre boş olamaz.\"}");
        return;
    }

    // Kullanıcı adı ve şifre uzunluk kontrolü
    if (u.length() > 50 || p.length() > 100) {
        addLog("Aşırı uzun giriş denemesi", WARN, "AUTH");
        server.send(400, "application/json", "{\"error\":\"Geçersiz giriş bilgileri.\"}");
        return;
    }

    if (u == settings.username) {
        String hashedAttempt = sha256(p, settings.passwordSalt);
        if (hashedAttempt == settings.passwordHash) {
            settings.isLoggedIn = true;
            settings.sessionStartTime = millis();
            loginAttempts = 0;
            lockoutTime = 0;
            
            addLog("✅ Başarılı giriş: " + u, SUCCESS, "AUTH");
            server.sendHeader("Location", "/", true);
            server.send(302, "text/plain", "Redirecting to dashboard..."); // İçerik eklendi
            return;
        }
    }

    // Başarısız giriş işlemi
    loginAttempts++;
    addLog("❌ Başarısız giriş denemesi (#" + String(loginAttempts) + "): " + u, ERROR, "AUTH");

    // Maksimum deneme sayısına ulaşıldı mı?
    if (loginAttempts >= MAX_LOGIN_ATTEMPTS) {
        lockoutTime = millis() + LOCKOUT_DURATION;
        addLog("🔒 IP adresi " + String(LOCKOUT_DURATION/1000) + " saniye kilitlendi", WARN, "AUTH");
        server.send(429, "application/json", 
            "{\"error\":\"Çok fazla başarısız deneme. " + String(LOCKOUT_DURATION/1000) + " saniye sonra tekrar deneyin.\"}");
        return;
    }

    server.send(401, "application/json", "{\"error\":\"Kullanıcı adı veya şifre hatalı!\"}");
}

void handleUserLogout() {
    if (settings.isLoggedIn) {
        settings.isLoggedIn = false;
        addLog("🚪 Çıkış yapıldı", INFO, "AUTH");
    }
    server.sendHeader("Location", "/login", true);
    server.send(302, "text/plain", "Redirecting to login..."); // İçerik eklendi
}

// Session yenileme fonksiyonu
void refreshSession() {
    if (settings.isLoggedIn) {
        settings.sessionStartTime = millis();
    }
}