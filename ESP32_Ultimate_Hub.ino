#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "lwip/lwip_napt.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- NÚT ENTER SƠ ĐỒ NRFBOX ---
#define BTN_ENTER_PIN 34 // Đổi thành GPIO tương ứng trên sơ đồ NRFBox của bạn (vd: 0, 34, 35, 13)

const int LED_PIN = 2;

String ap_ssid = "TunzAnhESP32_Setup";
String ap_pass = "12345678";
String sta_ssid = "";
String sta_pass = "";

String bot_token = "";
String chat_id = "";

String proxy_ip = "";
int proxy_port = 0;
String proxy_user = "";
String proxy_pass = "";
bool proxy_enabled = false;

IPAddress ap_ip(172, 20, 10, 1);
IPAddress ap_gateway(172, 20, 10, 1);
IPAddress ap_mask(255, 255, 255, 0);
IPAddress ap_leaseStart(172, 20, 10, 2);
IPAddress ap_dns(8, 8, 8, 8);

WebServer server(80);
WiFiServer proxyServer(8080);
DNSServer dnsServer;
Preferences preferences;

bool oledOK = false;
bool botStarted = false;
bool showQRMode = false;

unsigned long lastCronTime = 0;
const unsigned long CRON_INTERVAL = 1800000;
unsigned long lastWifiCheckTime = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;

// QR Code Zalo (0378389615) - Pixel Matrix 29x29
const uint8_t zalo_qr_bits[] PROGMEM = {
  0xff,0xff,0xff,0xf8,0x80,0x02,0x00,0x88,0xbd,0xbf,0x6d,0x88,0xbd,0xbf,0x6d,0x88,
  0xbd,0xbf,0x6d,0x88,0x80,0x02,0x00,0x88,0xff,0xff,0xff,0xf8,0x00,0x00,0x00,0x00,
  0xe3,0x8e,0x38,0xe8,0x1c,0xe3,0x8e,0x08,0x63,0x1c,0xe3,0x88,0x8e,0x38,0x1c,0xe8,
  0x38,0xe3,0x8e,0x08,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xf8,0x80,0x02,0x00,0x88,
  0xbd,0xbf,0x6d,0x88,0xbd,0xbf,0x6d,0x88,0xbd,0xbf,0x6d,0x88,0x80,0x02,0x00,0x88,
  0xff,0xff,0xff,0xf8
};

void displayMsg(String line1, String line2 = "", String line3 = "", String line4 = "") {
  if (!oledOK || showQRMode) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);  display.println(line1);
  display.setCursor(0, 16); display.println(line2);
  display.setCursor(0, 32); display.println(line3);
  display.setCursor(0, 48); display.println(line4);
  display.display();
}

void drawZaloQR() {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("  ZALO DEV SCAN QR"));
  
  // Vẽ QR ở giữa màn hình
  display.drawBitmap(49, 14, zalo_qr_bits, 29, 29, SSD1306_WHITE);
  
  display.setCursor(0, 48);
  display.println(F("Zalo: 0378389615"));
  display.setCursor(0, 56);
  display.println(F("Dev: Tunz Anh Dev"));
  display.display();
}

void sendTelegramMessage(String msg);

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      ip_napt_enable(ipaddr_addr("172.20.10.1"), 1);
      digitalWrite(LED_PIN, HIGH);
      displayMsg("SETUP AP MODE", "IP: 172.20.10.1", "Hay ket noi de cai da", "t");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ip_napt_enable(ipaddr_addr("172.20.10.1"), 0);
      digitalWrite(LED_PIN, LOW);
      displayMsg("SETUP AP MODE", "IP: 172.20.10.1", "Hay ket noi de cai da", "t");
      break;
    default: break;
  }
}

const char admin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>TunzAnhESP32 Kích Sóng</title>
  <style>
    body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0b0f19; color: #e2e8f0; margin: 0; padding: 15px; text-align: center; }
    .container { width: 100%; max-width: 500px; background: #151d30; padding: 20px; border-radius: 16px; margin: 0 auto; box-shadow: 0 10px 30px rgba(0,0,0,0.7); border: 1px solid #1e293b; box-sizing: border-box; }
    h2 { color: #38bdf8; margin: 0 0 5px 0; font-size: 22px; }
    .dev-badge { background: #0284c7; color: #fff; font-size: 11px; padding: 3px 10px; border-radius: 12px; display: inline-block; margin-bottom: 15px; font-weight: bold; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 15px; }
    .stat-box { background: #0b0f19; padding: 12px; border-radius: 10px; border: 1px solid #1e293b; text-align: left; }
    .stat-title { font-size: 11px; color: #64748b; font-weight: bold; text-transform: uppercase; }
    .stat-val { font-size: 14px; color: #38bdf8; font-weight: bold; margin-top: 4px; }
    .card { background: #0b0f19; padding: 16px; border-radius: 12px; margin-bottom: 15px; border: 1px solid #1e293b; text-align: left; }
    label { display: block; margin-bottom: 4px; font-size: 12px; color: #94a3b8; font-weight: bold; }
    input { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid #334155; background: #151d30; color: #fff; box-sizing: border-box; margin-bottom: 10px; font-size: 13px; }
    .btn { width: 100%; padding: 12px; background: #0284c7; color: #fff; border: none; border-radius: 8px; font-weight: bold; font-size: 14px; cursor: pointer; transition: 0.2s; }
    .btn:hover { background: #0369a1; }
    .btn-green { background: #16a34a; }
    .footer { font-size: 12px; color: #64748b; margin-top: 15px; line-height: 1.5; }
    .footer a { color: #38bdf8; text-decoration: none; font-weight: bold; }
  </style>
</head>
<body>
  <div class="container">
    <h2>⚡ TUNZANH ESP32 KÍCH SÓNG</h2>
    <div class="dev-badge">DEVELOPER EDITION</div>

    <div class="grid">
      <div class="stat-box"><div class="stat-title">CPU Speed</div><div class="stat-val" id="cpu">240 MHz</div></div>
      <div class="stat-box"><div class="stat-title">Free RAM</div><div class="stat-val" id="ram">Loading...</div></div>
      <div class="stat-box"><div class="stat-title">SoftAP IP</div><div class="stat-val">172.20.10.1</div></div>
      <div class="stat-box"><div class="stat-title">STA Status</div><div class="stat-val" id="sta">Loading...</div></div>
    </div>

    <div class="card">
      <form action="/save-wifi" method="POST">
        <label>Tên WiFi Nhà (SSID):</label>
        <input type="text" name="ssid" placeholder="Nhập tên WiFi..." required>
        <label>Mật Khẩu WiFi:</label>
        <input type="password" name="pass" placeholder="Nhập mật khẩu...">
        <hr style="border: 0.5px solid #1e293b; margin: 12px 0;">
        <label>Telegram Bot Token:</label>
        <input type="text" name="token" placeholder="Token Bot Telegram">
        <label>Telegram Chat ID:</label>
        <input type="text" name="chatid" placeholder="Chat ID nhận tin nhắn">
        <button type="submit" class="btn">LƯU CẤU HÌNH & RECONNECT</button>
      </form>
    </div>

    <div class="card">
      <label><b>HTTP Proxy Server (Port 8080)</b></label>
      <form action="/save-proxy" method="POST">
        <label>Proxy Forward IP:</label>
        <input type="text" name="px_ip" placeholder="IP hoặc Domain">
        <label>Proxy Port:</label>
        <input type="number" name="px_port" placeholder="8080">
        <button type="submit" class="btn btn-green">BẬT PROXY</button>
      </form>
    </div>

    <div class="card">
      <label><b>Nạp Firmware (.bin mới)</b></label>
      <form method="POST" action="/update" enctype="multipart/form-data">
        <input type="file" name="update" accept=".bin" required>
        <button type="submit" class="btn" style="background:#e11d48; margin-top:8px;">FLASH FIRMWARE</button>
      </form>
    </div>

    <div class="footer">
      Project: <b>TunzAnhESP32 Kích Sóng</b><br>
      Tác giả: <b>Tunz Anh Dev</b> | Zalo: <a href="https://zalo.me/0378389615" target="_blank">0378389615</a>
    </div>
  </div>

  <script>
    function updateStats() {
      fetch('/api/stats').then(r => r.json()).then(d => {
        document.getElementById('ram').innerText = (d.free_heap / 1024).toFixed(1) + ' KB';
        document.getElementById('sta').innerText = d.connected ? d.ip : 'Chưa kết nối';
      });
    }
    setInterval(updateStats, 3000);
    updateStats();
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240); // Ép xung 240MHz xử lý max tốc độ

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(BTN_ENTER_PIN, INPUT_PULLUP);

  Network.onEvent(onEvent);

  Wire.begin(21, 22);
  Wire.setClock(400000); // 400kHz I2C Bus
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    oledOK = true;
  }

  displayMsg("SETUP AP MODE", "IP: 172.20.10.1", "Hay ket noi de cai da", "t");

  preferences.begin("hub-config", true);
  sta_ssid = preferences.getString("ssid", "");
  sta_pass = preferences.getString("pass", "");
  bot_token = preferences.getString("token", "");
  chat_id = preferences.getString("chatid", "");
  preferences.end();

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(WIFI_PS_NONE);

  WiFi.mode(WIFI_AP_STA);
  WiFi.AP.begin();
  WiFi.AP.config(ap_ip, ap_gateway, ap_mask, ap_leaseStart, ap_dns);
  WiFi.AP.create(ap_ssid.c_str(), ap_pass.c_str());

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", ap_ip);
  proxyServer.begin();

  if (sta_ssid != "") {
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  }

  if (bot_token != "" && chat_id != "") {
    botStarted = true;
  }

  // --- WEB ROUTES ---
  server.on("/", []() { server.send_P(200, "text/html", admin_html); });
  
  // JSON API lấy thông số phần cứng real-time cho UI
  server.on("/api/stats", []() {
    String json = "{";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/save-wifi", HTTP_POST, []() {
    if (server.hasArg("ssid")) {
      preferences.begin("hub-config", false);
      preferences.putString("ssid", server.arg("ssid"));
      preferences.putString("pass", server.arg("pass"));
      preferences.putString("token", server.arg("token"));
      preferences.putString("chatid", server.arg("chatid"));
      preferences.end();
      server.send(200, "text/html", "<h3>Da luu! ESP32 đang khoi dong lai...</h3>");
      delay(1000);
      ESP.restart();
    }
  });

  server.on("/update", HTTP_POST, []() {
    bool success = !Update.hasError();
    server.send(200, "text/html", success ? "<h3>Flash thanh cong! Rebooting...</h3>" : "<h3>Flash thất bại!</h3>");
    if (success) { delay(1000); ESP.restart(); }
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { Serial.printf("Update Success: %uB\n", upload.totalSize); }
    }
  });

  server.onNotFound([]() { server.send_P(200, "text/html", admin_html); });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // BÓC NÚT ENTER SƠ ĐỒ NRFBOX DISPLAY QR CODE ZALO
  if (digitalRead(BTN_ENTER_PIN) == LOW) {
    showQRMode = !showQRMode;
    if (showQRMode) {
      drawZaloQR();
    } else {
      displayMsg("SETUP AP MODE", "IP: 172.20.10.1", "Hay ket noi de cai da", "t");
    }
    delay(300); // Chống nảy nút (Debounce)
  }

  // Tự động Reconnect giữ sóng
  if (millis() - lastWifiCheckTime >= WIFI_CHECK_INTERVAL) {
    lastWifiCheckTime = millis();
    if (sta_ssid != "" && WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    }
  }

  // Telegram Cron
  if (botStarted && WiFi.status() == WL_CONNECTED && (millis() - lastCronTime >= CRON_INTERVAL)) {
    lastCronTime = millis();
    String report = "⏰ **TUNZ ANH ESP32 REPORT**\n";
    report += "📡 Wi-Fi: " + sta_ssid + "\n";
    report += "🌐 IP: " + WiFi.localIP().toString() + "\n";
    report += "🧠 Free RAM: " + String(ESP.getFreeHeap()) + " bytes\n";
    sendTelegramMessage(report);
  }

  vTaskDelay(1 / portTICK_PERIOD_MS);
}

void sendTelegramMessage(String msg) {
  if (bot_token == "" || chat_id == "") return;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(2);
  if (client.connect("api.telegram.org", 443)) {
    String payload = "{\"chat_id\":\"" + chat_id + "\",\"text\":\"" + msg + "\",\"parse_mode\":\"Markdown\"}";
    client.println("POST /bot" + bot_token + "/sendMessage HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(payload.length());
    client.println("Connection: close");
    client.println();
    client.println(payload);
    client.stop();
  }
}
