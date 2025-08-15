#include "password_policy.h"
#include <Preferences.h>
#include "settings.h"
#include "log_system.h"
#include "crypto_utils.h"
#include "auth_system.h"  // checkSession için
#include <WebServer.h>

extern WebServer server;
extern Settings settings;

// Global password policy değişkeni (header'da extern olarak tanımlı)
PasswordPolicy passwordPolicy = {
    .firstLoginPasswordChange = true,
    .passwordExpiry = true,
    .passwordExpiryDays = 90,
    .minPasswordLength = 8,
    .requireComplexPassword = true,
    .lastPasswordChange = 0,
    .isDefaultPassword = true,
    .passwordHistory = 3
};

// Parola politikasını yükle
void loadPasswordPolicy() {
    Preferences prefs;
    prefs.begin("pwd-policy", true);
    
    passwordPolicy.firstLoginPasswordChange = prefs.getBool("first_change", true);
    passwordPolicy.passwordExpiry = prefs.getBool("expiry", true);
    passwordPolicy.passwordExpiryDays = prefs.getInt("expiry_days", 90);
    passwordPolicy.minPasswordLength = prefs.getInt("min_length", 8);
    passwordPolicy.requireComplexPassword = prefs.getBool("complex", true);
    passwordPolicy.lastPasswordChange = prefs.getULong("last_change", 0);
    passwordPolicy.isDefaultPassword = prefs.getBool("is_default", true);
    passwordPolicy.passwordHistory = prefs.getInt("history", 3);
    
    prefs.end();
    
    addLog("Parola politikası yüklendi", INFO, "POLICY");
}

// Parola politikasını kaydet
void savePasswordPolicy() {
    Preferences prefs;
    prefs.begin("pwd-policy", false);
    
    prefs.putBool("first_change", passwordPolicy.firstLoginPasswordChange);
    prefs.putBool("expiry", passwordPolicy.passwordExpiry);
    prefs.putInt("expiry_days", passwordPolicy.passwordExpiryDays);
    prefs.putInt("min_length", passwordPolicy.minPasswordLength);
    prefs.putBool("complex", passwordPolicy.requireComplexPassword);
    prefs.putULong("last_change", passwordPolicy.lastPasswordChange);
    prefs.putBool("is_default", passwordPolicy.isDefaultPassword);
    prefs.putInt("history", passwordPolicy.passwordHistory);
    
    prefs.end();
}

// Parola karmaşıklık kontrolü
bool isPasswordComplex(const String& password) {
    if (password.length() < passwordPolicy.minPasswordLength) {
        return false;
    }
    
    if (!passwordPolicy.requireComplexPassword) {
        return true;
    }
    
    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;
    bool hasSpecial = false;
    
    for (char c : password) {
        if (c >= 'A' && c <= 'Z') hasUpper = true;
        else if (c >= 'a' && c <= 'z') hasLower = true;
        else if (c >= '0' && c <= '9') hasDigit = true;
        else if (strchr("!@#$%^&*()_+-=[]{}|;:,.<>?", c)) hasSpecial = true;
    }
    
    // En az 3 farklı karakter türü olmalı
    int complexity = hasUpper + hasLower + hasDigit + hasSpecial;
    return complexity >= 3;
}

// Parola geçmişi kontrolü
bool isPasswordInHistory(const String& password) {
    Preferences prefs;
    prefs.begin("pwd-history", true);
    
    // Son 3 parola hash'ini kontrol et
    for (int i = 0; i < passwordPolicy.passwordHistory; i++) {
        String key = "pwd_" + String(i);
        String oldHash = prefs.getString(key.c_str(), "");
        
        if (oldHash.length() > 0) {
            // Salt'ı al
            String saltKey = "salt_" + String(i);
            String oldSalt = prefs.getString(saltKey.c_str(), "");
            
            // Yeni parolayı eski salt ile hashle ve karşılaştır
            if (sha256(password, oldSalt) == oldHash) {
                prefs.end();
                return true; // Parola daha önce kullanılmış
            }
        }
    }
    
    prefs.end();
    return false;
}

// Parola geçmişine ekle
void addPasswordToHistory(const String& passwordHash, const String& salt) {
    Preferences prefs;
    prefs.begin("pwd-history", false);
    
    // Mevcut parolaları bir kademe kaydır
    for (int i = passwordPolicy.passwordHistory - 1; i > 0; i--) {
        String keyFrom = "pwd_" + String(i - 1);
        String keyTo = "pwd_" + String(i);
        String saltKeyFrom = "salt_" + String(i - 1);
        String saltKeyTo = "salt_" + String(i);
        
        String hash = prefs.getString(keyFrom.c_str(), "");
        String oldSalt = prefs.getString(saltKeyFrom.c_str(), "");
        
        if (hash.length() > 0) {
            prefs.putString(keyTo.c_str(), hash);
            prefs.putString(saltKeyTo.c_str(), oldSalt);
        }
    }
    
    // Yeni parolayı ilk sıraya ekle
    prefs.putString("pwd_0", passwordHash);
    prefs.putString("salt_0", salt);
    
    prefs.end();
}

// Parola süresi dolmuş mu?
bool isPasswordExpired() {
    if (!passwordPolicy.passwordExpiry) {
        return false;
    }
    
    if (passwordPolicy.lastPasswordChange == 0) {
        return true; // Hiç değiştirilmemiş
    }
    
    unsigned long daysSinceChange = (millis() - passwordPolicy.lastPasswordChange) / 86400000;
    return daysSinceChange >= passwordPolicy.passwordExpiryDays;
}

// Parola değiştirme zorunluluğu kontrolü
bool mustChangePassword() {
    // Varsayılan parola kullanılıyorsa
    if (passwordPolicy.isDefaultPassword && passwordPolicy.firstLoginPasswordChange) {
        return true;
    }
    
    // Parola süresi dolmuşsa
    if (isPasswordExpired()) {
        return true;
    }
    
    return false;
}

// Web handler - Parola değiştirme sayfası
void handlePasswordChangePage() {
    if (!checkSession()) {
        server.sendHeader("Location", "/login");
        server.send(302);
        return;
    }
    
    String html = R"(
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Parola Değiştir - TEİAŞ EKLİM</title>
    <link rel="stylesheet" href="/style.css">
    <style>
        .password-change-container {
            max-width: 500px;
            margin: 50px auto;
            padding: 2rem;
            background: var(--bg-primary);
            border-radius: var(--radius);
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .password-requirements {
            background: var(--bg-secondary);
            padding: 1rem;
            border-radius: var(--radius);
            margin: 1rem 0;
        }
        .requirement {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            margin: 0.5rem 0;
        }
        .requirement.met {
            color: var(--success);
        }
        .requirement.unmet {
            color: var(--error);
        }
        .password-strength {
            height: 8px;
            background: var(--bg-secondary);
            border-radius: 4px;
            margin: 1rem 0;
            overflow: hidden;
        }
        .password-strength-bar {
            height: 100%;
            transition: width 0.3s, background-color 0.3s;
        }
        .strength-weak { background: var(--error); width: 33%; }
        .strength-medium { background: var(--warning); width: 66%; }
        .strength-strong { background: var(--success); width: 100%; }
    </style>
</head>
<body>
    <div class="password-change-container">
        <h1>🔐 Parola Değiştirme Zorunlu</h1>
        <p class="warning-message">
            )";
    
    if (passwordPolicy.isDefaultPassword) {
        html += "İlk girişinizde varsayılan parolayı değiştirmeniz gerekmektedir.";
    } else if (isPasswordExpired()) {
        html += "Parolanızın süresi dolmuştur. Güvenliğiniz için yeni bir parola belirlemeniz gerekmektedir.";
    }
    
    html += R"(
        </p>
        
        <form id="passwordChangeForm" method="POST" action="/api/change-password">
            <div class="form-group">
                <label for="currentPassword">Mevcut Parola</label>
                <input type="password" id="currentPassword" name="currentPassword" required>
            </div>
            
            <div class="form-group">
                <label for="newPassword">Yeni Parola</label>
                <input type="password" id="newPassword" name="newPassword" required 
                       minlength=")" + String(passwordPolicy.minPasswordLength) + R"(">
            </div>
            
            <div class="password-strength">
                <div id="strengthBar" class="password-strength-bar"></div>
            </div>
            
            <div class="form-group">
                <label for="confirmPassword">Yeni Parola (Tekrar)</label>
                <input type="password" id="confirmPassword" name="confirmPassword" required>
            </div>
            
            <div class="password-requirements">
                <h4>Parola Gereksinimleri:</h4>
                <div class="requirement" id="reqLength">
                    <span id="reqLengthIcon">❌</span>
                    <span>En az )" + String(passwordPolicy.minPasswordLength) + R"( karakter</span>
                </div>
                <div class="requirement" id="reqUpper">
                    <span id="reqUpperIcon">❌</span>
                    <span>En az bir büyük harf</span>
                </div>
                <div class="requirement" id="reqLower">
                    <span id="reqLowerIcon">❌</span>
                    <span>En az bir küçük harf</span>
                </div>
                <div class="requirement" id="reqDigit">
                    <span id="reqDigitIcon">❌</span>
                    <span>En az bir rakam</span>
                </div>
                <div class="requirement" id="reqSpecial">
                    <span id="reqSpecialIcon">❌</span>
                    <span>En az bir özel karakter (!@#$%^&*)</span>
                </div>
            </div>
            
            <button type="submit" class="btn primary" id="submitBtn" disabled>
                Parolayı Değiştir
            </button>
        </form>
    </div>
    
    <script>
        const newPasswordInput = document.getElementById('newPassword');
        const confirmPasswordInput = document.getElementById('confirmPassword');
        const submitBtn = document.getElementById('submitBtn');
        const strengthBar = document.getElementById('strengthBar');
        
        function checkPasswordRequirements() {
            const password = newPasswordInput.value;
            let meetsAll = true;
            
            // Uzunluk kontrolü
            const lengthMet = password.length >= )" + String(passwordPolicy.minPasswordLength) + R"(;
            document.getElementById('reqLength').className = lengthMet ? 'requirement met' : 'requirement unmet';
            document.getElementById('reqLengthIcon').textContent = lengthMet ? '✅' : '❌';
            if (!lengthMet) meetsAll = false;
            
            // Büyük harf kontrolü
            const upperMet = /[A-Z]/.test(password);
            document.getElementById('reqUpper').className = upperMet ? 'requirement met' : 'requirement unmet';
            document.getElementById('reqUpperIcon').textContent = upperMet ? '✅' : '❌';
            if (!upperMet) meetsAll = false;
            
            // Küçük harf kontrolü
            const lowerMet = /[a-z]/.test(password);
            document.getElementById('reqLower').className = lowerMet ? 'requirement met' : 'requirement unmet';
            document.getElementById('reqLowerIcon').textContent = lowerMet ? '✅' : '❌';
            if (!lowerMet) meetsAll = false;
            
            // Rakam kontrolü
            const digitMet = /[0-9]/.test(password);
            document.getElementById('reqDigit').className = digitMet ? 'requirement met' : 'requirement unmet';
            document.getElementById('reqDigitIcon').textContent = digitMet ? '✅' : '❌';
            if (!digitMet) meetsAll = false;
            
            // Özel karakter kontrolü
            const specialMet = /[!@#$%^&*()_+\-=\[\]{}|;:,.<>?]/.test(password);
            document.getElementById('reqSpecial').className = specialMet ? 'requirement met' : 'requirement unmet';
            document.getElementById('reqSpecialIcon').textContent = specialMet ? '✅' : '❌';
            if (!specialMet) meetsAll = false;
            
            // Güç göstergesi
            const strength = (lengthMet + upperMet + lowerMet + digitMet + specialMet) / 5;
            if (strength <= 0.4) {
                strengthBar.className = 'password-strength-bar strength-weak';
            } else if (strength <= 0.7) {
                strengthBar.className = 'password-strength-bar strength-medium';
            } else {
                strengthBar.className = 'password-strength-bar strength-strong';
            }
            
            // Butonu etkinleştir/devre dışı bırak
            const passwordsMatch = newPasswordInput.value === confirmPasswordInput.value;
            submitBtn.disabled = !meetsAll || !passwordsMatch || newPasswordInput.value.length === 0;
        }
        
        newPasswordInput.addEventListener('input', checkPasswordRequirements);
        confirmPasswordInput.addEventListener('input', checkPasswordRequirements);
    </script>
</body>
</html>
    )";
    
    server.send(200, "text/html", html);
}

// API handler - Parola değiştirme
void handlePasswordChangeAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    String currentPassword = server.arg("currentPassword");
    String newPassword = server.arg("newPassword");
    String confirmPassword = server.arg("confirmPassword");
    
    // Mevcut parola kontrolü
    String hashedCurrent = sha256(currentPassword, settings.passwordSalt);
    if (hashedCurrent != settings.passwordHash) {
        server.send(400, "application/json", "{\"error\":\"Mevcut parola yanlış\"}");
        addLog("❌ Parola değiştirme başarısız: Yanlış mevcut parola", ERROR, "AUTH");
        return;
    }
    
    // Yeni parolaların eşleşme kontrolü
    if (newPassword != confirmPassword) {
        server.send(400, "application/json", "{\"error\":\"Yeni parolalar eşleşmiyor\"}");
        return;
    }
    
    // Parola karmaşıklık kontrolü
    if (!isPasswordComplex(newPassword)) {
        server.send(400, "application/json", "{\"error\":\"Parola gereksinimleri karşılanmıyor\"}");
        return;
    }
    
    // Parola geçmişi kontrolü
    if (isPasswordInHistory(newPassword)) {
        server.send(400, "application/json", "{\"error\":\"Bu parola daha önce kullanılmış\"}");
        return;
    }
    
    // Yeni parolayı kaydet
    String newSalt = generateSalt(16);
    String newHash = sha256(newPassword, newSalt);
    
    // Geçmişe ekle
    addPasswordToHistory(newHash, newSalt);
    
    // Settings'i güncelle
    settings.passwordSalt = newSalt;
    settings.passwordHash = newHash;
    
    // Preferences'a kaydet
    Preferences prefs;
    prefs.begin("app-settings", false);
    prefs.putString("p_salt", newSalt);
    prefs.putString("p_hash", newHash);
    prefs.end();
    
    // Politikayı güncelle
    passwordPolicy.isDefaultPassword = false;
    passwordPolicy.lastPasswordChange = millis();
    savePasswordPolicy();
    
    addLog("✅ Parola başarıyla değiştirildi", SUCCESS, "AUTH");
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Parola değiştirildi\"}");
    
    // Oturumu sonlandır
    settings.isLoggedIn = false;
}