// TEİAŞ EKLİM Modern JavaScript - Çok Sayfalı Mimarisine Uygun Versiyon

document.addEventListener('DOMContentLoaded', () => {
    // --- GENEL UYGULAMA MANTIĞI ---

    const state = {
        ws: null,
        wsConnected: false,
        reconnectTimer: null,
        reconnectAttempts: 0,
        maxReconnectAttempts: 5,
        authenticated: false,
        logPaused: false,
        autoScroll: true
    };

    // --- WebSocket Yönetimi ---

    function connectWebSocket() {
        if (state.ws || state.reconnectAttempts >= state.maxReconnectAttempts) return;

        try {
            const wsUrl = `ws://${window.location.hostname}:81`;
            console.log('WebSocket bağlantısı deneniyor:', wsUrl);
            updateWSStatus(false, 'Bağlanıyor...');

            state.ws = new WebSocket(wsUrl);
            state.ws.onopen = onWsOpen;
            state.ws.onmessage = onWsMessage;
            state.ws.onclose = onWsClose;
            state.ws.onerror = onWsError;
        } catch (error) {
            console.error('WebSocket bağlantı hatası:', error);
            scheduleReconnect();
        }
    }

    function onWsOpen() {
        console.log('WebSocket bağlandı');
        state.wsConnected = true;
        state.reconnectAttempts = 0;
        updateWSStatus(true, 'Bağlı');
        
        // Basit bir kimlik doğrulama token'ı gönder
        state.ws.send(JSON.stringify({ cmd: 'auth', token: 'session_' + Date.now() }));
    }

    function onWsMessage(event) {
        try {
            const data = JSON.parse(event.data);
            console.log('WS Mesajı:', data);

            switch (data.type) {
                case 'auth_success':
                    state.authenticated = true;
                    console.log('WebSocket kimlik doğrulama başarılı');
                    // Gerekli verileri iste
                    if (document.getElementById('logContainer')) {
                         state.ws.send(JSON.stringify({ cmd: 'get_logs' }));
                    }
                    if (document.querySelector('.status-grid')) {
                         state.ws.send(JSON.stringify({ cmd: 'get_status' }));
                    }
                    break;
                case 'status':
                    updateSystemStatus(data);
                    break;
                case 'log':
                    if (!state.logPaused) addLogEntry(data);
                    break;
                case 'error':
                     showMessage(data.message, 'error');
                     break;
            }
        } catch (error) {
            console.error('WebSocket mesaj parse hatası:', error);
        }
    }

    function onWsClose(event) {
        console.log('WebSocket bağlantısı kapandı:', event.code, event.reason);
        state.ws = null;
        state.wsConnected = false;
        state.authenticated = false;
        updateWSStatus(false, 'Bağlantı Yok');

        if (event.code !== 1000) { // 1000 = Normal kapanış
            scheduleReconnect();
        }
    }

    function onWsError(error) {
        console.error('WebSocket hatası:', error);
        updateWSStatus(false, 'Hata');
        if (state.ws) {
            state.ws.close();
        }
    }

    function scheduleReconnect() {
        if (state.reconnectTimer) return;
        
        state.reconnectAttempts++;
        if (state.reconnectAttempts > state.maxReconnectAttempts) {
             console.log("Maksimum yeniden bağlanma denemesine ulaşıldı.");
             return;
        }

        const delay = Math.min(1000 * Math.pow(2, state.reconnectAttempts), 15000);
        console.log(`Yeniden bağlanma ${delay}ms sonra denenecek...`);

        state.reconnectTimer = setTimeout(() => {
            state.reconnectTimer = null;
            connectWebSocket();
        }, delay);
    }
    
    function sendWsMessage(data) {
        if (state.ws && state.ws.readyState === WebSocket.OPEN && state.authenticated) {
            state.ws.send(JSON.stringify(data));
        } else {
            console.warn('WS bağlantısı hazır değil, mesaj gönderilemedi:', data);
        }
    }

    // --- ARAYÜZ GÜNCELLEME FONKSİYONLARI ---
    
    function updateElement(id, value) {
        const element = document.getElementById(id);
        if (element && value !== undefined && value !== null) {
            element.textContent = value;
        }
    }

    function updateWSStatus(connected, text) {
        const statusDot = document.querySelector('.status-dot');
        const wsState = document.getElementById('wsState');
        if (wsState) wsState.textContent = text;
        if (statusDot) {
            statusDot.style.background = connected ? 'var(--success)' : 'var(--error)';
            statusDot.style.animation = connected ? 'pulse 2s infinite' : 'none';
        }
    }
    
    function showMessage(text, type = 'info', duration = 3000) {
        const container = document.getElementById('message-container');
        if (!container) return;
        
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${type}`;
        messageDiv.textContent = text;
        container.appendChild(messageDiv);
        
        setTimeout(() => {
            messageDiv.remove();
        }, duration);
    }

    function addLogEntry(logData) {
        const logContainer = document.getElementById('logContainer');
        if (!logContainer) return;
        
        // Yükleniyor... mesajını kaldır
        const loading = logContainer.querySelector('.loading-logs');
        if (loading) loading.remove();

        const logEntry = document.createElement('div');
        logEntry.className = `log-entry log-${logData.level.toLowerCase()}`;
        logEntry.innerHTML = `
            <span class="log-time">${logData.timestamp}</span>
            <span class="log-level">${logData.level}</span>
            <span class="log-source">${logData.source}</span>
            <span class="log-message">${logData.message}</span>
        `;
        
        logContainer.prepend(logEntry);
        
        if (state.autoScroll) {
            logContainer.scrollTop = 0;
        }

        while (logContainer.children.length > 200) { // Limiti 200 yapalım
            logContainer.removeChild(logContainer.lastChild);
        }
    }

    // --- Sayfa Spesifik Fonksiyonlar ---
    
    // Genel Ayarlar Sayfası (account.html)
    function initAccountPage() {
        const form = document.getElementById('accountForm');
        if (!form) return;

        // Mevcut ayarları yükle
        fetch('/api/settings').then(r => r.json()).then(settings => {
            updateElement('deviceName', settings.deviceName);
            updateElement('tmName', settings.tmName);
            updateElement('username', settings.username);
        });

        form.addEventListener('submit', (e) => {
            e.preventDefault();
            const formData = new FormData(form);
            const newPassword = formData.get('password');
            const confirmPassword = formData.get('confirmPassword');

            if (newPassword && newPassword !== confirmPassword) {
                showMessage('Yeni şifreler eşleşmiyor!', 'error');
                return;
            }

            fetch('/api/settings', { method: 'POST', body: new URLSearchParams(formData) })
                .then(response => {
                    if (response.ok) {
                        showMessage('Ayarlar başarıyla kaydedildi.', 'success');
                    } else {
                        showMessage('Ayarlar kaydedilemedi.', 'error');
                    }
                });
        });
    }

    // NTP Ayarları Sayfası (ntp.html)
    function initNtpPage() {
        const form = document.getElementById('ntpForm');
        if (!form) return;

        // Mevcut ayarları yükle
        fetch('/api/ntp').then(r => r.json()).then(ntp => {
             updateElement('currentServer1', ntp.ntpServer1);
             updateElement('currentServer2', ntp.ntpServer2);
             document.getElementById('ntpServer1').value = ntp.ntpServer1;
             document.getElementById('ntpServer2').value = ntp.ntpServer2;
        });

        form.addEventListener('submit', (e) => {
            e.preventDefault();
            const formData = new FormData(form);
            fetch('/api/ntp', { method: 'POST', body: new URLSearchParams(formData) })
                .then(response => {
                    if (response.ok) {
                        showMessage('NTP ayarları başarıyla gönderildi.', 'success');
                    } else {
                        showMessage('NTP ayarları gönderilemedi.', 'error');
                    }
                });
        });
    }
    
    // BaudRate Sayfası (baudrate.html)
    function initBaudRatePage() {
        const form = document.getElementById('baudrateForm');
        if (!form) return;

        fetch('/api/baudrate').then(r => r.json()).then(br => {
             updateElement('currentBaudRate', br.baudRate + ' bps');
             const radio = document.querySelector(`input[name="baud"][value="${br.baudRate}"]`);
             if (radio) radio.checked = true;
        });

        form.addEventListener('submit', (e) => {
            e.preventDefault();
            const formData = new FormData(form);
             fetch('/api/baudrate', { method: 'POST', body: new URLSearchParams(formData) })
                .then(response => {
                    if (response.ok) {
                        showMessage('BaudRate başarıyla değiştirildi.', 'success');
                         setTimeout(() => location.reload(), 1000);
                    } else {
                        showMessage('BaudRate değiştirilemedi.', 'error');
                    }
                });
        });
    }
    
    // Arıza Kayıtları Sayfası (fault.html)
    function initFaultPage() {
        const firstFaultBtn = document.getElementById('firstFaultBtn');
        const nextFaultBtn = document.getElementById('nextFaultBtn');
        const faultContent = document.getElementById('faultContent');

        if (!firstFaultBtn) return;
        
        const fetchFault = (endpoint) => {
            fetch(endpoint, { method: 'POST' })
            .then(r => r.text())
            .then(text => {
                const emptyState = faultContent.querySelector('.empty-state');
                if(emptyState) emptyState.remove();
                
                const recordDiv = document.createElement('div');
                recordDiv.className = 'fault-record';
                recordDiv.textContent = text;
                faultContent.prepend(recordDiv);
                updateElement('faultCommStatus', 'Başarılı');
                document.getElementById('faultCommStatus').className = 'value status-badge active';
            }).catch(() => {
                 updateElement('faultCommStatus', 'Başarısız');
                 document.getElementById('faultCommStatus').className = 'value status-badge error';
            });
        };

        firstFaultBtn.addEventListener('click', () => fetchFault('/api/faults/first'));
        nextFaultBtn.addEventListener('click', () => fetchFault('/api/faults/next'));
    }

    // Log Sayfası (log.html)
    function initLogPage() {
        const pauseBtn = document.getElementById('pauseLogsBtn');
        const clearBtn = document.getElementById('clearLogsBtn');
        const autoScrollBtn = document.getElementById('autoScrollToggle');

        if (!pauseBtn) return;

        pauseBtn.addEventListener('click', () => {
            state.logPaused = !state.logPaused;
            pauseBtn.textContent = state.logPaused ? '▶️ Devam Ettir' : '⏸️ Duraklat';
        });

        clearBtn.addEventListener('click', () => {
             document.getElementById('logContainer').innerHTML = '';
        });

        autoScrollBtn.addEventListener('click', () => {
            state.autoScroll = !state.autoScroll;
            autoScrollBtn.dataset.active = state.autoScroll;
        });
    }

    // --- UYGULAMA BAŞLATMA ---
    function init() {
        // Her sayfada çalışacaklar
        if (window.location.pathname !== '/login.html') {
             connectWebSocket();
        }

        // Sayfa spesifik başlatıcıları çağır
        initAccountPage();
        initNtpPage();
        initBaudRatePage();
        initFaultPage();
        initLogPage();
    }
    
    init();
});