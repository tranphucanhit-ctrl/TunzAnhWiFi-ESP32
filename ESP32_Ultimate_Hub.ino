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

const int LED_PIN = 2;

String ap_ssid = "TunzAnhESP32_KichSong";
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

WebServer server(80);
WiFiServer proxyServer(8080);
DNSServer dnsServer;
Preferences preferences;

bool oledOK = false;
bool botStarted = false;

volatile bool wifiConnectedEvent = false;
volatile bool wifiDisconnectedEvent = false;

unsigned long lastCronTime = 0;
const unsigned long CRON_INTERVAL = 1800000;

unsigned long lastWifiCheckTime = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;

// Bộ đếm thời gian cập nhật OLED liên tục
unsigned long lastOLEDRefreshTime = 0;
const unsigned long OLED_REFRESH_INTERVAL = 1000; // Cập nhật mỗi 1 giây (1000ms)

void displayMsg(String line1, String line2 = "", String line3 = "", String line4 = "") {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);  display.println(line1);
  display.setCursor(0, 16); display.println(line2);
  display.setCursor(0, 32); display.println(line3);
  display.setCursor(0, 48); display.println(line4);
  display.display();
}

// Hàm render UI OLED thời gian thực
void updateRealtimeOLED() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setCursor(0, 0);
  display.println(F("TunzAnhESP32 KichSong"));

  // IP / Status
  display.setCursor(0, 14);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("IP:" + WiFi.localIP().toString());
  } else {
    display.println(F("IP: Disconnected"));
  }

  // AP / Mode
  display.setCursor(0, 28);
  display.println(F("AP:172.20.10.1 (NAPT)"));

  // Thông số RAM + Thời gian chạy Uptime (giây)
  display.setCursor(0, 42);
  display.println("RAM:" + String(ESP.getFreeHeap() / 1024) + "KB | " + String(millis() / 1000) + "s");

  // Footer Developer
  display.setCursor(0, 54);
  display.println(F("Dev: Tunz Anh Dev"));

  display.display();
}

void sendTelegramMessage(String msg);

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      ip_napt_enable(ipaddr_addr("172.20.10.1"), 1);
      wifiConnectedEvent = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ip_napt_enable(ipaddr_addr("172.20.10.1"), 0);
      wifiDisconnectedEvent = true;
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
    body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; text-align: center; }
    .container { width: 100%; max-width: 480px; background: #1e293b; padding: 24px; border-radius: 16px; margin: 0 auto; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid #334155; }
    h2 { color: #38bdf8; margin-bottom: 20px; }
    .card { background: #0f172a; padding: 16px; border-radius: 10px; margin-bottom: 16px; border: 1px solid #334155; text-align: left; }
    label { display: block; margin-bottom: 6px; font-size: 13px; color: #94a3b8; font-weight: bold; }
    input[type="file"], input[type="text"], input[type="password"], input[type="number"] { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid #334155; background: #1e293b; color: #fff; box-sizing: border-box; margin-bottom: 10px; }
    .btn { width: 100%; padding: 12px; background: #38bdf8; color: #0f172a; border: none; border-radius: 8px; font-weight: bold; font-size: 15px; cursor: pointer; margin-top: 5px; }
    .btn-green { background: #22c55e; color: #fff; }
    hr { border: 0.5px solid #334155; margin: 15px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h2>⚡ TUNZANH ESP32 KÍCH SÓNG</h2>
    <div class="card">
      <form action="/save-wifi" method="POST">
        <label>Tên WiFi nhà (SSID):</label>
        <input type="text" name="ssid" placeholder="Nhập tên WiFi..." required>
        <label>Mật khẩu WiFi:</label>
        <input type="password" name="pass" placeholder="Nhập mật khẩu...">
        <hr>
        <label>Bot Token Telegram:</label>
        <input type="text" name="token" placeholder="123456:ABC-DEF1234...">
        <label>Chat ID Telegram:</label>
        <input type="text" name="chatid" placeholder="987654321">
        <button type="submit" class="btn">LƯU WIFI & TELEGRAM</button>
      </form>
    </div>
    <div class="card">
      <form action="/save-proxy" method="POST">
        <label><b>Cấu hình Proxy Client:</b></label>
        <label>IP Proxy Server:</label>
        <input type="text" name="px_ip" placeholder="IP hoặc Domain" required>
        <label>Port Proxy:</label>
        <input type="number" name="px_port" placeholder="8080" required>
        <label>Username (Nếu có):</label>
        <input type="text" name="px_user" placeholder="Proxy User">
        <label>Password (Nếu có):</label>
        <input type="password" name="px_pass" placeholder="Proxy Password">
        <button type="submit" class="btn btn-green">CHẠY PROXY</button>
      </form>
    </div>
    <div class="card">
      <label><b>Nạp Firmware (.bin mới)</b></label>
      <form method="POST" action="/update" enctype="multipart/form-data">
        <input type="file" name="update" accept=".bin" required>
        <button type="submit" class="btn">FLASH FIRMWARE</button>
      </form>
    </div>
  </div>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.onEvent(onEvent);

  Wire.begin(21, 22);
  Wire.setClock(400000);
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    oledOK = true;
    displayMsg("TunzAnhESP32 KichSong", "  Dang khoi tao...", "  IP: 172.20.10.1", "  Dev: Tunz Anh Dev");
  }

  preferences.begin("hub-config", true);
  sta_ssid = preferences.getString("ssid", "");
  sta_pass = preferences.getString("pass", "");
  bot_token = preferences.getString("token", "");
  chat_id = preferences.getString("chatid", "");
  proxy_ip = preferences.getString("px_ip", "");
  proxy_port = preferences.getInt("px_port", 0);
  proxy_user = preferences.getString("px_user", "");
  proxy_pass = preferences.getString("px_pass", "");
  proxy_enabled = preferences.getBool("px_en", false);
  preferences.end();

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleep(WIFI_PS_NONE);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(ap_ip, ap_gateway, ap_mask);
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", ap_ip);

  proxyServer.begin();

  if (sta_ssid != "") {
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  }

  if (bot_token != "" && chat_id != "") {
    botStarted = true;
  }

  server.on("/", []() { server.send_P(200, "text/html", admin_html); });
  server.on("/hotspot-detect.html", []() { server.send_P(200, "text/html", admin_html); });

  server.on("/save-wifi", HTTP_POST, []() {
    if (server.hasArg("ssid")) {
      preferences.begin("hub-config", false);
      preferences.putString("ssid", server.arg("ssid"));
      preferences.putString("pass", server.arg("pass"));
      preferences.putString("token", server.arg("token"));
      preferences.putString("chatid", server.arg("chatid"));
      preferences.end();
      server.send(200, "text/html", "<h3>Da luu! ESP32 dang khoi dong lai...</h3>");
      delay(1000);
      ESP.restart();
    }
  });

  server.on("/save-proxy", HTTP_POST, []() {
    if (server.hasArg("px_ip") && server.hasArg("px_port")) {
      preferences.begin("hub-config", false);
      preferences.putString("px_ip", server.arg("px_ip"));
      preferences.putInt("px_port", server.arg("px_port").toInt());
      preferences.putString("px_user", server.arg("px_user"));
      preferences.putString("px_pass", server.arg("px_pass"));
      preferences.putBool("px_en", true);
      preferences.end();
      server.send(200, "text/html", "<h3>Da luu Proxy! Rebooting...</h3>");
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

  // Bật/Tắt LED theo trạng thái kết nối
  if (wifiConnectedEvent) {
    wifiConnectedEvent = false;
    digitalWrite(LED_PIN, HIGH);
  }

  if (wifiDisconnectedEvent) {
    wifiDisconnectedEvent = false;
    digitalWrite(LED_PIN, LOW);
  }

  // CẬP NHẬT MÀN HÌNH OLED LIÊN TỤC MỖI 1 GIÂY
  if (millis() - lastOLEDRefreshTime >= OLED_REFRESH_INTERVAL) {
    lastOLEDRefreshTime = millis();
    updateRealtimeOLED();
  }

  // Kiếm tra định kỳ kết nối Wi-Fi
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
    String report = "⏰ **BÁO CÁO ĐỊNH KỲ (30 PHÚT)**\n";
    report += "📡 TunzAnhESP32 Kích Sóng\n";
    report += "🌐 Local IP: " + WiFi.localIP().toString() + "\n";
    report += "🧠 RAM Trống: " + String(ESP.getFreeHeap()) + " bytes\n";
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

    unsigned long startWait = millis();
    while (client.connected() && millis() - startWait < 1000) {
      while (client.available()) { client.read(); }
    }
    client.stop();
  }
}
